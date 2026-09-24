#!/usr/bin/env python3

"""Test password entry on a real terminal (a pty).

Line editing (erase, kill, werase) must work while typing a password,
and the terminal's echo must come back if password entry is
interrupted by ^C or ^Z.
"""

import fcntl
import os
import signal
import sys
import tempfile
import termios
import time
import traceback

SRD = os.path.abspath('./srd')
TIMEOUT = 10  # seconds


class Failure(Exception):
    pass


class Terminal:
    """A pty whose session we lead, as a shell does.

    srd runs as the foreground job in its own process group, so ^C
    and ^Z reach it the way they reach a user's srd.  (A bare session
    leader would not do: its process group is orphaned, and the
    kernel discards stop signals sent to orphaned groups.)
    """

    def __init__(self):
        os.setsid()
        self.master, self.slave = os.openpty()
        fcntl.ioctl(self.slave, termios.TIOCSCTTY, 0)
        attrs = termios.tcgetattr(self.slave)
        attrs[3] |= termios.ICANON | termios.ECHO | termios.ISIG
        attrs[6][termios.VERASE] = b'\x7f'
        attrs[6][termios.VKILL] = b'\x15'
        attrs[6][termios.VWERASE] = b'\x17'
        attrs[6][termios.VINTR] = b'\x03'
        attrs[6][termios.VSUSP] = b'\x1a'
        termios.tcsetattr(self.slave, termios.TCSANOW, attrs)
        # Taking the terminal back from a job would otherwise stop us.
        signal.signal(signal.SIGTTOU, signal.SIG_IGN)
        self.output = b''

    def start(self, args):
        self.output = b''
        pid = os.fork()
        if 0 == pid:
            os.setpgid(0, 0)
            os.tcsetpgrp(self.slave, os.getpid())
            signal.signal(signal.SIGTTOU, signal.SIG_DFL)
            for fd in (0, 1, 2):
                os.dup2(self.slave, fd)
            os.execv(SRD, [SRD] + args)
        os.setpgid(pid, pid)
        os.tcsetpgrp(self.slave, pid)
        self.pid = pid

    def drain(self):
        os.set_blocking(self.master, False)
        try:
            while True:
                self.output += os.read(self.master, 4096)
        except (BlockingIOError, OSError):
            pass

    def until(self, what, condition):
        deadline = time.time() + TIMEOUT
        while not condition():
            if time.time() > deadline:
                raise Failure('Timed out waiting for ' + what +
                              '.  Output so far:  ' + repr(self.output))
            time.sleep(0.01)
            self.drain()

    def echo_on(self):
        return bool(termios.tcgetattr(self.slave)[3] & termios.ECHO)

    def type_password(self, prompt, keys):
        """Wait for the prompt and for echo to go off, then type."""
        self.until('prompt ' + repr(prompt), lambda: prompt in self.output)
        self.until('echo off', lambda: not self.echo_on())
        self.output = b''
        os.write(self.master, keys)

    def wait(self, stopped_ok=False):
        """Wait for srd to exit (or stop) and return its wait status."""
        flags = os.WNOHANG | (os.WUNTRACED if stopped_ok else 0)
        result = []

        def done():
            pid, status = os.waitpid(self.pid, flags)
            if pid:
                result.append(status)
            return bool(result)

        self.until('srd to finish', done)
        if not os.WIFSTOPPED(result[0]):
            os.tcsetpgrp(self.slave, os.getpgrp())
        return result[0]


def expect_exit(status, code, what):
    if not os.WIFEXITED(status) or os.WEXITSTATUS(status) != code:
        raise Failure('%s:  expected exit %d, got wait status %#x'
                      % (what, code, status))


def run_tests(term, db_dir):
    args = ['--database-dir', db_dir, '-m', '']

    term.start(args[:2] + ['--create'] + args[2:])
    term.type_password(b'Password:', b'secret\n')
    term.type_password(b'retype', b'secret\n')
    expect_exit(term.wait(), 0, 'Create database')

    for name, keys, code in [
            ('Plain password', b'secret\n', 0),
            ('Wrong password', b'secrets\n', 1),
            ('Erase (backspace)', b'secrex\x7ft\n', 0),
            ('Kill (^U)', b'garbage\x15secret\n', 0),
            ('Word erase (^W)', b'junk\x17secret\n', 0)]:
        term.start(args)
        term.type_password(b'Password:', keys)
        expect_exit(term.wait(), code, name)
        if not term.echo_on():
            raise Failure(name + ':  echo left off.')

    # ^C at the prompt:  srd dies, and echo is back on.
    term.start(args)
    term.type_password(b'Password:', b'sec\x03')
    status = term.wait()
    if not (os.WIFSIGNALED(status) and
            signal.SIGINT == os.WTERMSIG(status)):
        raise Failure('^C:  expected death by SIGINT, got wait status %#x'
                      % status)
    if not term.echo_on():
        raise Failure('^C:  echo left off.')

    # ^Z at the prompt:  echo is on while srd is stopped, off again when
    # it continues, and the password still works.
    term.start(args)
    term.type_password(b'Password:', b'sec\x1a')
    status = term.wait(stopped_ok=True)
    if not os.WIFSTOPPED(status):
        raise Failure('^Z:  expected srd to stop, got wait status %#x'
                      % status)
    if not term.echo_on():
        raise Failure('^Z:  echo off while stopped.')
    os.kill(term.pid, signal.SIGCONT)
    term.until('echo off after continuing', lambda: not term.echo_on())
    os.write(term.master, b'\x15secret\n')
    expect_exit(term.wait(), 0, '^Z then continue')


def main():
    # setsid() fails for a process group leader, which we may be when
    # run from an interactive shell, so run the tests in a child.
    pid = os.fork()
    if 0 == pid:
        code = 0
        try:
            with tempfile.TemporaryDirectory() as db_dir:
                run_tests(Terminal(), db_dir)
        except Failure as e:
            print('test-tty:  ' + str(e))
            code = 1
        except Exception:
            traceback.print_exc()
            code = 1
        sys.stdout.flush()
        sys.stderr.flush()
        os._exit(code)
    _, status = os.waitpid(pid, 0)
    sys.exit(os.WEXITSTATUS(status) if os.WIFEXITED(status) else 1)


if __name__ == '__main__':
    main()
