/*
  Copyright 2011  Jeff Abrahamson

  This file is part of srd.

  srd is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  srd is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with srd.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <boost/program_options.hpp>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <set>
#include <signal.h>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "srd.h"

namespace BPO = boost::program_options;
using namespace srd;
using namespace std;

class help_exception : public exception {};

namespace {
string get_password(const string prompt = "Password:  ");

BPO::variables_map parse_options(int argc, char *argv[]) {
  BPO::options_description general("General options");
  general.add_options()("help,h", "Produce help message")(
      "read-only,R", "Read-only, do not persist data")(
      "database-dir", BPO::value<string>(),
      "Name of directory to use for database instead of default")(
      "verbose,v", "Emit debugging information");

  BPO::options_description actions("Actions (if none, then match)");
  actions.add_options()
      /*("shell,s",
        "Run interactive shell")*/
      ("edit,e", "Edit record")(
          "delete,x", "Delete the results of an (exact) key match.  "
                      "Conceptually -E -m arg, can be used with -mDd.")(
          "delete-all,X",
          "Delete all matches.  "
          "Conceptually equivalent to -m arg, can be used with -mDd.  "
          "If more than one match, will request confirmation.")(
          "passwd,p", "Change password (no other options permitted)")(
          "upgrade",
          "Convert a database in srd's old, weaker format to the current "
          "one, keeping its password (no other options permitted).  "
          "Changing the password also converts it.")(
          "create",
          "Create database.  The database is identified by a hash of the "
          "password, so --create distinguishes between creating a new "
          "database and mistyping the password.")(
          "import", BPO::value<string>(),
          "Import a text file.  The format is the same as that output "
          "by -f:  a line \"[KEY]\" starts a record with key KEY, and each "
          "following line, less its two spaces of indent, is a line of that "
          "record's data (payload).  An empty line is an empty line of data.  "
          "Keys that already exist are pointed out before confirming; "
          "import never replaces a record.")
              ("validate,V",
               "Confirm that all records are loadable and consistent")(
                  "checksum", "Compute and display whole database checksum "
                              "(all keys and payloads together)")(
                  "checksum-by-key", "Compute and display database checksum by "
                                     "key, restrictable by matching options");

  BPO::options_description matching("Matching options");
  matching.add_options()("match-key,m", BPO::value<vector_string>(),
                         "Restrict to records whose keys match")(
      "match-data-or-key,D", BPO::value<vector_string>(),
      "Restrict to records whose key or data match")(
      "match-data,d", BPO::value<vector_string>(),
      "Restrict to records whose data match all patterns (cf. -J)")(
      "exact-match,E", "Exact key match")(
      "ignore-case,i", "Match without case.  (Does not affect -g,--grep.)")(
      "disjunction,J",
      "When matching multiple patterns against payloads with "
      "-d, --match-data, use inclusive or (any hit is sufficient)");

  BPO::options_description display("Display options");
  display.add_options()("keys-only,k", "Display keys only (default true if "
                                       "multiple matches, false for single "
                                       "match)")(
      "full-display,f",
      "Display full data (default true for single match, false for multiple "
      "matches)")("grep,g", BPO::value<string>(), "Output filter for data");

  BPO::options_description test(
      "Test options (don't use outside regression tests)");
  test.add_options()(
      "TEST,T", BPO::value<string>(),
      "Test mode, use local data directory, specify password on commandline");

  BPO::options_description options("Allowed options");
  options.add(general).add(actions).add(matching).add(display).add(test);

  BPO::positional_options_description pos;
  pos.add("match-key", -1);

  BPO::variables_map opt_map;
  BPO::store(BPO::command_line_parser(argc, argv)
                 .options(options)
                 .positional(pos)
                 .run(),
             opt_map);
  BPO::notify(opt_map);

  if (opt_map.count("help")) {
    cout << "srd <options> [key-match-pattern]" << endl << endl;
    cout << "  Pattern matching options (-m, -D, -d) may be repeated." << endl;
    cout << "    Multiple key patterns (-m) match as inclusive or." << endl;
    cout << "    Multiple payload patterns (-d) match as and (but cf. -J)."
         << endl;
    cout << "    Multiple key-or-payload patterns (-D) match as inclusive or."
         << endl;
    cout << "  Key-match-pattern, if present, is the same as specifying -m."
         << endl;
    cout << "  To create a new record, use -e with no pattern." << endl;
    cout << endl;

    cout << options << endl;
    throw help_exception();
  }

  return opt_map;
}

/*
  Read a line from stdin, without its newline.

  Read a byte at a time with read(2), so that no copy of the line is
  left in a stdio or iostream buffer, and nothing past the newline is
  consumed, so later reads from cin see the rest of the input.
  Reserve space up front so that growing the string doesn't leave
  copies behind (short of an absurdly long line).
*/
string read_secret_line() {
  string line;
  line.reserve(4096);
  char c;
  while (true) {
    ssize_t n = read(STDIN_FILENO, &c, 1);
    if (n < 0 && EINTR == errno)
      continue;
    if (n <= 0 || '\n' == c)
      break;
    line.push_back(c);
  }
  c = 0;
  return line;
}

/*
  Turn off terminal echo while a password is typed, and make sure it
  comes back on.

  Only ECHO is cleared.  The terminal stays in canonical mode, so the
  user's line editing (erase, kill, werase, ...) works as usual.

  If a signal would kill us mid-prompt, restore the terminal first, so
  the user's shell isn't left without echo.  On ^Z, restore it, stop,
  and turn echo off again when continued.  Signals that were ignored
  when we started stay ignored.

  The signal handlers need the terminal settings, hence file statics.
  Only one instance may exist at a time.
*/
termios g_echo_termios, g_noecho_termios;

void set_handler(int sig, void (*handler)(int)) {
  struct sigaction sa = {};
  sa.sa_handler = handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  sigaction(sig, &sa, nullptr);
}

void on_fatal_signal(int sig) {
  tcsetattr(STDIN_FILENO, TCSANOW, &g_echo_termios);
  set_handler(sig, SIG_DFL);
  raise(sig); // Delivered, and fatal, once this handler returns.
}

void on_stop_signal(int sig) {
  int saved_errno = errno;
  tcsetattr(STDIN_FILENO, TCSANOW, &g_echo_termios);
  set_handler(sig, SIG_DFL);
  sigset_t unblock;
  sigemptyset(&unblock);
  sigaddset(&unblock, sig);
  sigprocmask(SIG_UNBLOCK, &unblock, nullptr);
  raise(sig); // We stop here until continued.
  set_handler(sig, on_stop_signal);
  tcsetattr(STDIN_FILENO, TCSANOW, &g_noecho_termios);
  errno = saved_errno;
}

class NoEchoTerminal {
public:
  NoEchoTerminal() {
    m_active = isatty(STDIN_FILENO) && 0 == tcgetattr(STDIN_FILENO,
                                                         &g_echo_termios);
    if (!m_active)
      return;
    g_noecho_termios = g_echo_termios;
    g_noecho_termios.c_lflag &= ~ECHO;
    for (size_t i = 0; i < num_signals; ++i) {
      sigaction(signals[i], nullptr, &m_old_actions[i]);
      if (SIG_IGN != m_old_actions[i].sa_handler)
        set_handler(signals[i],
                    SIGTSTP == signals[i] ? on_stop_signal : on_fatal_signal);
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &g_noecho_termios);
  }
  ~NoEchoTerminal() {
    if (!m_active)
      return;
    tcsetattr(STDIN_FILENO, TCSANOW, &g_echo_termios);
    for (size_t i = 0; i < num_signals; ++i)
      sigaction(signals[i], &m_old_actions[i], nullptr);
  }
  NoEchoTerminal(const NoEchoTerminal &) = delete;
  NoEchoTerminal &operator=(const NoEchoTerminal &) = delete;

private:
  static constexpr int signals[] = {SIGHUP, SIGINT, SIGQUIT, SIGTERM,
                                    SIGTSTP};
  static constexpr size_t num_signals = sizeof(signals) / sizeof(signals[0]);
  bool m_active;
  struct sigaction m_old_actions[num_signals];
};

/*
  Wipe a string (a passphrase) when it goes out of scope.
*/
class Wiper {
public:
  explicit Wiper(string &s) : m_s(s) {}
  ~Wiper() { explicit_bzero(m_s.data(), m_s.size()); }
  Wiper(const Wiper &) = delete;
  Wiper &operator=(const Wiper &) = delete;

private:
  string &m_s;
};

string get_password(const string prompt) {
  // Prompt on stderr, so that a caller capturing stdout (e.g., to feed
  // a record to another program) gets only the records and the user
  // still sees the prompt.  Flush cout first so earlier output isn't
  // reordered after the prompt.
  cout << flush;
  cerr << prompt << flush;

  string pass_phrase;
  {
    NoEchoTerminal no_echo;
    pass_phrase = read_secret_line();
  }
  cerr << endl;
  // The root derives keys from the pass phrase.  It can't be hashed
  // here, because the salt is in the database directory.
  return pass_phrase;
}

/*
  Change password.

  If successful, remove old database (root and its leaves).

  Return 0 on success.
  Return 1 on failure.
*/
bool change_password(const string &password) {
  Root old_root(password, "");

  string passwd;
  Wiper wipe_passwd(passwd);
  if (mode(Testing)) {
    // When testing, password is in the clear on the commandline
    // and we surely don't want to ask again.
    passwd =
        password +
        "2"; // If this line changs, will need to change pass2 in test-passwd.sh
  } else {
    passwd = get_password("Enter new password:  ");
    if (passwd == password) {
      cout << "New and old passwords are identical." << endl;
      return 1;
    }
    string passwd2 = get_password("Please retype new password:  ");
    Wiper wipe_passwd2(passwd2);
    if (passwd != passwd2) {
      cout << "Passwords don't match." << endl;
      return 1;
    }
  }
  try {
    Root new_root = old_root.change_password(passwd);
  } catch (const runtime_error& e) {
    cerr << "Failed to change password:  " << e.what() << endl;
    return 1;
  }
  return 0;
}

/*
  Rewrite a format 1 database in format 2, keeping its password.

  Return 0 on success (including when there's nothing to do).
  Return 1 on failure.
*/
bool do_upgrade(const string &password) {
  try {
    Root root(password, "");
    if (Key::Format1 != root.format()) {
      cout << "The database is already in the current format." << endl;
      return 0;
    }
    root.upgrade(password);
  } catch (const runtime_error &e) {
    cerr << "Failed to upgrade the database:  " << e.what() << endl;
    return 1;
  }
  cout << "Database upgraded." << endl;
  return 0;
}

/* **********************************************************************
   Edit / new record.
   **********************************************************************
   */

LeafProxyMap get_leaf_proxy_map(Root &root, const vector_string &match_key,
                                const vector_string &match_data,
                                const vector_string &match_or,
                                const bool match_exact, const bool disjunction,
                                const StringMatcher &in_matcher);
void user_add(Root &root);
bool user_edit(string &key, string &payload);

class LeafVisitor {

public:
  virtual ~LeafVisitor(){};
  virtual void operator()(const LeafProxyMap &lpm) const = 0;
};

/*
  A visitor that prints the results of a search.
*/
class LeafPrintVisitor : public LeafVisitor {

public:
  LeafPrintVisitor(const bool in_keys_only, const bool in_full_display,
                   const string in_grep)
      : keys_only(in_keys_only), full_display(in_full_display), grep(in_grep){};

  void operator()(const LeafProxyMap &lpm) const {
    /*
      If one hit
      ...and key_display, display only key.
      ...and full_display, display all data.
      ...else display full data.

      If multiple hits
      ...and key_display, display only keys.
      ...and full_display, display all data.
      ...else display only keys.
    */
    bool full_display_on =
        ((full_display && lpm.size() > 1) || (!keys_only && 1 == lpm.size()));
    LeafProxyMap::LeafProxyList leaves = lpm.sorted_by_key();
    for (LeafProxyMap::LeafProxyList::const_iterator it = leaves.begin();
         it != leaves.end(); ++it) {
      (*it).print_key();
      if (full_display_on)
        (*it).print_payload(grep);
    }
  }

private:
  const bool keys_only;
  const bool full_display;
  const string grep;
};

/*
  A visitor that deletes the results of a search.

  If the set has more than one element, request verification
  before deleting.
*/
class LeafDeleteVisitor : public LeafVisitor {

public:
  LeafDeleteVisitor(Root &in_root) : the_root(in_root){};

  void operator()(const LeafProxyMap &lpm) const {
    if (0 == lpm.size()) {
      cout << "Nothing to delete." << endl;
      return;
    }
    if (lpm.size() > 1) {
      cout << "Delete " << lpm.size() << " items?  (yes/no)  ";
      string response;
      cin >> response;
      if ("y" != response && "Y" != response && "yes" != response &&
          "YES" != response && "Yes" != response) {
        cout << "OK, nothing deleted." << endl;
        return;
      }
    }
    for (LeafProxyMap::const_iterator it = lpm.begin(); it != lpm.end(); ++it)
      the_root.rm_leaf(it->first);
  }

private:
  Root &the_root;
};

/*
  A visitor that prints each record's key and a checksum of its payload.
*/
class LeafChecksumVisitor : public LeafVisitor {

public:
  LeafChecksumVisitor(const Root &in_root) : the_root(in_root){};

  void operator()(const LeafProxyMap &lpm) const {
    // Sort on content alone, so that the output doesn't depend on the
    // (random) file names of records that share a key.
    vector<pair<string, string>> lines;
    for (LeafProxyMap::const_iterator it = lpm.begin(); it != lpm.end(); ++it)
      lines.emplace_back(it->second.key(),
                         the_root.checksum_of(it->second.payload()));
    sort(lines.begin(), lines.end());
    for (const auto &[key, digest] : lines)
      cout << "[" << key << "]  " << digest << endl;
  }

private:
  const Root &the_root;
};


/*
  Utility function to provide a reference to a StringMatcher (via operator*).
*/
const boost::shared_ptr<StringMatcher>
string_matcher(const bool case_sensitive) {
  if (case_sensitive)
    return boost::shared_ptr<IdentStringMatcher>(new IdentStringMatcher());
  return boost::shared_ptr<UpperStringMatcher>(new UpperStringMatcher());
}

void do_edit(const string &password, const vector_string &match_key,
             const vector_string &match_payload, const vector_string &match_or,
             const bool match_exact, const bool disjunction,
             const StringMatcher &in_matcher) {
  if (mode(ReadOnly)) {
    cerr << "Database is read-only.  Edit not permitted." << endl;
    return;
  }
  Root root(password, "");
  if (mode(ReadOnly)) {
    // Maybe instantiating the root discovered
    // that we don't have write permission.
    cerr << "Database is read-only.  Edit not permitted." << endl;
    return;
  }

  if (0 == match_key.size() && 0 == match_payload.size() &&
      0 == match_or.size()) {
    // Edit request with no search criteria means create new
    user_add(root);
    return;
  }
  LeafProxyMap lpm =
      get_leaf_proxy_map(root, match_key, match_payload, match_or, match_exact,
                         disjunction, in_matcher);
  if (0 == lpm.size()) {
    cout << "No record matches." << endl;
    return;
  }
  if (1 == lpm.size()) {
    string proxy_key = lpm.begin()->first;
    LeafProxy lp = lpm.begin()->second;
    string key = lp.key();
    string payload = lp.payload();
    if (!user_edit(key, payload))
      return;
    root.set_leaf(proxy_key, key, payload);
    return;
  }

  for (LeafProxyMap::iterator it = lpm.begin(); it != lpm.end(); ++it)
    it->second.print_key();

  return;
}

void do_match(Root &root, const vector_string &match_key,
              const vector_string &match_payload, const vector_string &match_or,
              const bool match_exact, const bool disjunction,
              const StringMatcher &in_matcher, LeafVisitor *visitor) {
  if (0 == match_key.size() && 0 == match_payload.size() &&
      0 == match_or.size()) {
    cerr << "No match criteria provided." << endl;
    cerr << "To match all records, use an empty key (\"\")." << endl;
    return;
  }
  LeafProxyMap lpm =
      get_leaf_proxy_map(root, match_key, match_payload, match_or, match_exact,
                         disjunction, in_matcher);

  (*visitor)(lpm);
}

LeafProxyMap get_leaf_proxy_map(Root &root, const vector_string &match_key,
                                const vector_string &match_payload,
                                const vector_string &match_or,
                                const bool match_exact, const bool disjunction,
                                const StringMatcher &in_matcher) {
  LeafProxyMap lpm = root.filter_keys(match_key, match_exact, in_matcher);
  if (match_payload.size())
    lpm = lpm.filter_payloads(match_payload, disjunction, in_matcher);
  if (match_or.size())
    lpm = lpm.filter_keys_or_payloads(match_or, match_exact, in_matcher);
  return lpm;
}

/*
  User interaction to add a new leaf to the root.
*/
void user_add(Root &root) {
  string key;
  string payload;
  if (user_edit(key, payload))
    root.add_leaf(key, payload);
}

/*
  A private scratch directory in which to edit a record in the clear.
  mkdtemp() creates it mode 0700, so neither the record nor any swap
  or backup files the editor writes beside it are visible to other
  users.  Everything in it is removed on destruction.
*/
class PrivateTempDir {
public:
  PrivateTempDir() {
    string pattern =
        (std::filesystem::temp_directory_path() / "srd-XXXXXX").string();
    vector<char> buf(pattern.begin(), pattern.end());
    buf.push_back('\0');
    if (!mkdtemp(buf.data()))
      throw(runtime_error(string("Failed to create temporary directory:  ") +
                          strerror(errno)));
    m_path = buf.data();
  }
  ~PrivateTempDir() {
    std::error_code ec;
    std::filesystem::remove_all(m_path, ec);
    if (ec)
      cerr << "Failed to remove " << m_path << ":  " << ec.message() << endl;
  }
  PrivateTempDir(const PrivateTempDir &) = delete;
  PrivateTempDir &operator=(const PrivateTempDir &) = delete;

  const string &path() const { return m_path; }

private:
  string m_path;
};

/*
  Keep srd alive while the editor runs, so that we always get to
  remove the plaintext copy of the record.

  Like system(), ignore SIGINT and SIGQUIT: a ^C typed in the editor
  goes to the whole foreground process group.  Block SIGHUP and
  SIGTERM: if one arrives, it is delivered when this object is
  destroyed, which must be after the temporary directory is removed.

  The editor itself gets the original signal handling back via
  restore() before exec.
*/
class EditorSignalGuard {
public:
  EditorSignalGuard() {
    struct sigaction ignore = {};
    ignore.sa_handler = SIG_IGN;
    sigemptyset(&ignore.sa_mask);
    sigaction(SIGINT, &ignore, &m_old_int);
    sigaction(SIGQUIT, &ignore, &m_old_quit);
    sigset_t block;
    sigemptyset(&block);
    sigaddset(&block, SIGHUP);
    sigaddset(&block, SIGTERM);
    sigprocmask(SIG_BLOCK, &block, &m_old_mask);
  }
  ~EditorSignalGuard() { restore(); }
  EditorSignalGuard(const EditorSignalGuard &) = delete;
  EditorSignalGuard &operator=(const EditorSignalGuard &) = delete;

  // Async-signal-safe, so callable in the child between fork and exec.
  void restore() const {
    sigaction(SIGINT, &m_old_int, nullptr);
    sigaction(SIGQUIT, &m_old_quit, nullptr);
    sigprocmask(SIG_SETMASK, &m_old_mask, nullptr);
  }

private:
  struct sigaction m_old_int, m_old_quit;
  sigset_t m_old_mask;
};

/*
  Run the user's $EDITOR on filename and wait for it.
  Return true if it exited successfully.

  EDITOR may carry arguments ("emacs -nw"), so, as git does, let the
  shell split it, but pass the filename as a positional parameter so
  that the shell never parses it.
*/
bool run_editor(const string &filename, const EditorSignalGuard &guard) {
  const char *editor = getenv("EDITOR");
  if (!editor || !*editor) {
    cerr << "EDITOR is not defined, we'll try vi." << endl;
    editor = "vi";
  }
  string command = string(editor) + " \"$@\"";

  pid_t pid = fork();
  if (pid < 0) {
    perror("Failed to start editor");
    return false;
  }
  if (0 == pid) {
    guard.restore();
    execl("/bin/sh", "sh", "-c", command.c_str(), "sh", filename.c_str(),
          static_cast<char *>(nullptr));
    _exit(127);
  }
  int status;
  while (waitpid(pid, &status, 0) < 0) {
    if (EINTR != errno) {
      perror("Failed waiting for editor");
      return false;
    }
  }
  if (WIFEXITED(status) && 0 == WEXITSTATUS(status))
    return true;
  if (WIFEXITED(status))
    cerr << "Editor exited with status " << WEXITSTATUS(status) << "." << endl;
  else
    cerr << "Editor terminated abnormally." << endl;
  return false;
}

/*
  Call the user's $EDITOR on key/payload.
  Return true if modified, false otherwise.
*/
bool user_edit(string &key, string &payload) {
  ostringstream sdata;
  sdata << "[" << key << "]" << endl << payload;
  string data = sdata.str();

  // Declaration order matters: the directory must be removed before
  // the guard lets any pending SIGHUP or SIGTERM through.
  EditorSignalGuard guard;
  PrivateTempDir dir;
  string filename = dir.path() + "/record";
  file_create_private(filename, data);

  if (!run_editor(filename, guard))
    return false;

  ifstream fs(filename, ios::in | ios::binary);
  if (!fs.is_open()) {
    cerr << "Failed to read back the edited record." << endl;
    return false;
  }
  ostringstream new_data_ss;
  new_data_ss << fs.rdbuf();
  string new_data = new_data_ss.str();
  if (new_data == data)
    return false;
  size_t pos = new_data.find_first_of('\n');
  if (string::npos == pos) {
    cerr << "Failed to find end of key (end of first line)." << endl;
    return false;
  }
  string first_line(new_data.substr(0, pos));
  size_t first_length = first_line.size();
  if ('[' != first_line[0] || ']' != first_line[first_length - 1]) {
    cerr << "First line is corrupt: failed to find square brackets at "
            "extremities."
         << endl;
    cerr << first_line << endl;
    // We might hope to offer to re-invoke the editor
    return false;
  }
  first_line = first_line.substr(1, first_length - 2);
  string remainder(new_data.substr(pos + 1));
  key = first_line;
  payload = remainder;
  return true;
}

/* **********************************************************************
   File import.
   **********************************************************************
   */
vector<pair<string, string>> import_read_file(const string filename);

/*
  Import new records from a text file.
  Query the user to confirm correct parsing.
  Commit the change on user confirmation.
*/
bool do_import(const string &password, const string &filename) {
  vector<pair<string, string>> incoming = import_read_file(filename);
  if (0 == incoming.size()) {
    cout << "Import abandonned." << endl;
    return false;
  }

  // Point out keys that will end up on more than one record.  Import
  // never replaces a record.
  Root root(password, "");
  set<string> existing_keys;
  for (LeafProxyMap::const_iterator it = root.begin(); it != root.end(); ++it)
    existing_keys.insert(it->second.key());
  set<string> seen_keys;
  cout << "Found " << incoming.size() << " keys:" << endl;
  for (const auto &[key, payload] : incoming) {
    cout << "\t" << key;
    if (existing_keys.count(key))
      cout << "  (already exists)";
    else if (!seen_keys.insert(key).second)
      cout << "  (repeated in file)";
    cout << endl;
  }
  cout << "Import?  (yes/no)  ";
  string response;
  cin >> response;
  if ("y" != response && "Y" != response && "yes" != response &&
      "YES" != response && "Yes" != response) {
    cout << "OK, import abandonned." << endl;
    return false;
  }

  // Hold the lock across the whole batch, so that no other srd can
  // change the root between our first add and the commit.
  Lock lock(root.dirname());
  for (const auto &[key, payload] : incoming)
    root.add_leaf(key, payload, false);
  root.commit();
  return true;
}

/*
  Read and parse the import file.

  The format is what -f prints:  a line "[KEY]" starts a record, and
  each following line, less its two-space indent, is a line of that
  record's payload.  An empty line is an empty payload line whose
  indent an editor stripped as trailing whitespace.  A record may have
  no payload lines.

  Return a vector of (key, payload) pairs.
  On error (or empty file), return an empty vector.
*/
vector<pair<string, string>> import_read_file(const string filename) {
  ifstream fs(filename.c_str(), ios::in | ios::binary);
  if (!fs.is_open()) {
    cerr << "Failed to open \"" << filename << "\" for reading." << endl;
    return vector<pair<string, string>>();
  }

  vector<pair<string, string>> incoming;
  const string prefix("  ");
  string line;
  long line_num = 0;
  while (getline(fs, line)) {
    ++line_num;
    if (line.size() >= 2 && '[' == line.front() && ']' == line.back()) {
      incoming.emplace_back(line.substr(1, line.size() - 2), string());
    } else if (incoming.empty()) {
      cerr << "Error at line " << line_num
           << ": expected a [KEY] line before any data." << endl;
      return vector<pair<string, string>>();
    } else if (line.empty()) {
      incoming.back().second += "\n";
    } else if (0 != line.compare(0, prefix.size(), prefix)) {
      cerr << "Error at line " << line_num << ": expected two space indent."
           << endl;
      return vector<pair<string, string>>();
    } else {
      incoming.back().second += line.substr(prefix.size()) + "\n";
    }
  }
  if (0 == line_num)
    cout << "File is empty." << endl;
  return incoming;
}

/*
  Create a new root.

  Return 0 on success.
  Return 1 on failure.
*/
bool do_create(const string &password) {
  if (!mode(Testing)) {
    // When testing, password is in the clear on the commandline
    // and we surely don't want to ask again.
    string passwd2 = get_password("Please retype your password:  ");
    Wiper wipe_passwd2(passwd2);
    if (password != passwd2) {
      cout << "Passwords don't match." << endl;
      return 1;
    }
  }
  unique_ptr<Root> root;
  try {
    // Will fail if root already exists.
    root = make_unique<Root>(password, "", true);
  } catch (const runtime_error& e) {
    cerr << "Failed to create new database:  a database identified by this "
            "password already exists."
         << endl;
    if (mode(Verbose))
      cerr << "  ==> " << e.what() << endl;
    return 1;
  }
  root->commit();
  return 0;
}

/*
  Instantiate the root and force validation.
  Return zero if all is well, non-zero otherwise.
*/
bool do_validate(const string &password) {
  // Catch std::exception, not just runtime_error:  corrupt data can
  // surface as other types (bzip2 integrity errors are domain_error).
  try {
    Root root(password, "");
    try {
      root.validate(true);
    } catch (const exception &e) {
      cout << "Validation failed: forced validation failed." << endl;
      cout << e.what() << endl;
      return 1;
    }
  } catch (const exception &e) {
    cout << "Validation failed:  root did not instantiate." << endl;
    cout << e.what() << endl;
    return 1;
  }
  return 0;
}

/*
  Instantiate the root and print a checksum of all records.
  Return zero if all is well, non-zero otherwise.
*/
bool do_checksum(const string &password) {
  try {
    Root root(password, "");
    try {
      root.checksum();
    } catch (const exception &e) {
      cout << "Failed to compute checksum." << endl;
      cout << e.what() << endl;
      return 1;
    }
  } catch (const exception &e) {
    cout << "Checksum failed:  root did not instantiate." << endl;
    cout << e.what() << endl;
    return 1;
  }
  return 0;
}

/*
  Keep secrets out of core dumps.  Errors used to end in abort(), and
  a core file would then hold the passphrase, keys, and decrypted
  records.  PR_SET_DUMPABLE also stops other processes of the same
  user from attaching with ptrace.
*/
void disable_core_dumps() {
  struct rlimit no_core = {0, 0};
  if (setrlimit(RLIMIT_CORE, &no_core))
    perror("Warning:  failed to disable core dumps");
#ifdef __linux__
  if (prctl(PR_SET_DUMPABLE, 0, 0, 0, 0))
    perror("Warning:  failed to mark process non-dumpable");
#endif
}

/*
  Do that thing that we do.
*/
int run(int argc, char *argv[]) {
  BPO::variables_map options;
  try {
    options = parse_options(argc, argv);
  } catch (const help_exception&) {
    return 0; // User asked for help, done
  } catch (const exception &e) {
    // Something went wrong.  Say so and exit with error.
    cerr << e.what() << endl;
    // Options aren't available, so be verbose to be clear.
    cerr << "(Error is fatal, quitting before doing anything.)" << endl;
    return 1;
  }

  const bool verbose = (options.count("verbose") > 0);
  mode(Verbose, verbose);
  mode(ReadOnly, options.count("read-only") > 0);

  bool is_test = options.count("TEST") > 0;
  mode(Testing, is_test);
  bool match_case_sensitive = (options.count("ignore-case") == 0);
  bool disjunction = (options.count("disjunction") > 0);

  if (options.count("database-dir"))
    set_base_dir(options["database-dir"].as<string>());

  string passwd;
  Wiper wipe_passwd(passwd);
  if (is_test)
    passwd = options["TEST"].as<string>();
  else
    passwd = get_password();

  // Do something.
  if (options.count("create") > 0)
    if (do_create(passwd))
      return 1; // Password mismatch.

  if (options.count("validate") > 0)
    return (do_validate(passwd));

  if (options.count("checksum") > 0)
    return (do_checksum(passwd));

  if (options.count("passwd"))
    return change_password(passwd);
  if (options.count("upgrade"))
    return do_upgrade(passwd);
  if (options.count("import")) {
    string filename = options["import"].as<string>();
    do_import(passwd, filename);
    return 0;
  }

  // We need to match, either to edit a record or else to
  // display or delete records.
  vector_string match_key, match_or, match_data;
  bool match_exact = false;
  if (options.count("match-key")) {
    match_key = options["match-key"].as<vector_string>();
    if (verbose) {
      cout << "match-key=";
      copy(match_key.begin(), match_key.end(),
           ostream_iterator<string>(cout, ","));
      cout << endl;
    }
  }
  if (options.count("match-data-or-key")) {
    match_or = options["match-data-or-key"].as<vector_string>();
    if (verbose) {
      cout << "match-data-or-key=";
      copy(match_or.begin(), match_or.end(),
           ostream_iterator<string>(cout, ","));
      cout << endl;
    }
  }
  if (options.count("match-data")) {
    match_data = options["match-data"].as<vector_string>();
    if (verbose) {
      cout << "match-data=";
      copy(match_data.begin(), match_data.end(),
           ostream_iterator<string>(cout, ","));
      cout << endl;
    }
  }
  match_exact =
      (options.count("exact-match") > 0) || (options.count("delete") > 0);
  if (verbose)
    cout << "match-exact=" << match_exact << endl;

  if (options.count("edit")) {
    do_edit(passwd, match_key, match_data, match_or, match_exact, disjunction,
            *string_matcher(match_case_sensitive));
    return 0;
  }

  LeafVisitor *lv;

  Root root(passwd, "");
  if (options.count("delete")) {
    match_exact = true;
    lv = new LeafDeleteVisitor(root);
  } else if (options.count("delete-all")) {
    lv = new LeafDeleteVisitor(root);
  } else if (options.count("checksum-by-key")) {
    lv = new LeafChecksumVisitor(root);
    if (0 == match_key.size())
      match_key.push_back(""); // match all keys if no match requested
  } else {
    lv = new LeafPrintVisitor(
        options.count("keys-only") > 0, options.count("full-display") > 0,
        (options.count("grep") > 0) ? options["grep"].as<string>() : string());
  }
  do_match(root, match_key, match_data, match_or, match_exact, disjunction,
           *string_matcher(match_case_sensitive), lv);
  delete lv;
  return 0;
}
}

int main(int argc, char *argv[]) {
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  disable_core_dumps();
  int ret;
  try {
    ret = run(argc, argv);
  } catch (const exception &e) {
    cerr << "srd:  " << e.what() << endl;
    ret = 1;
  } catch (...) {
    cerr << "srd:  unexpected error" << endl;
    ret = 1;
  }
  google::protobuf::ShutdownProtobufLibrary();
  return ret;
}
