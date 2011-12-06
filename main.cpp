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
#include <iomanip>
#include <iostream>
//#include <crypto++/base64.h>
//#include <crypto++/sha.h>
#include <stdexcept>
#include <termios.h>

#include "interface.h"
#include "crypt.h"
#include "types.h"


namespace BPO = boost::program_options;
using namespace srd;
using namespace std;

class help_exception : public exception {};


static BPO::variables_map parse_options(int, char *[]);
static string get_password();
static void do_shell(const string, const bool);
static void change_password(const string, const bool);
static void do_edit(const string,
                    const vector_string,
                    const vector_string,
                    const vector_string,
                    bool,
                    bool);
static void do_match(const string,
                     const vector_string,
                     const vector_string,
                     const vector_string,
                     bool,
                     bool);


int main(int argc, char *argv[])
{
        BPO::variables_map options;
        try {
                options = parse_options(argc, argv);
        }
        catch(help_exception) {
                return 0;       // User asked for help, done
        }
        catch(exception& e) {
                // Something went wrong.  Say so and exit with error.
                cerr << e.what() << endl;
                // Options aren't available, so be verbose to be clear.
                cerr << "(Error is fatal, quitting before doing anything.)" << endl;
                return 1;
        }

        bool is_test = options.count("TEST") > 0;
        string passwd;
        if(is_test) {
                passwd = options["TEST"].as<string>();
                test_mode(true);
        } else
                passwd = get_password();
        
        // do something
        bool verbose = options.count("verbose") > 0;

        if(options.count("shell")) {
                do_shell(passwd, verbose);
                return 0;
        }
        if(options.count("passwd")) {
                change_password(passwd, verbose);
                return 0;
        }

        // We need to match, either to edit a record or to display one.
        vector_string match_key, match_or, match_data;
        bool match_exact = false;
        if(options.count("match-key")) {
                match_key = options["match-key"].as<vector_string>();
                if(verbose) {
                        cout << "match-key=";
                        copy(match_key.begin(), match_key.end(), ostream_iterator<string>(cout, ","));
                        cout << endl;
                }
        }
        if(options.count("match-data-or-key")) {
                match_or = options["match-data-or-key"].as<vector_string>();
                if(verbose) {
                        cout << "match-data-or-key=";
                        copy(match_or.begin(), match_or.end(), ostream_iterator<string>(cout, ","));
                        cout << endl;
                }
        }
        if(options.count("match-data")) {
                match_data = options["match-data"].as<vector_string>();
                if(verbose) {
                        cout << "match-data=";
                        copy(match_data.begin(), match_data.end(), ostream_iterator<string>(cout, ","));
                        cout << endl;
                }
        }
        match_exact = options.count("exact-match") > 0;
        if(verbose)
                cout << "match-exact=" << match_exact << endl;
        
        if(options.count("edit")) {
                do_edit(passwd, match_key, match_data, match_or, match_exact, verbose);
                return 0;
        }

        do_match(passwd, match_key, match_data, match_or, match_exact, verbose);
        return 0;
}



static BPO::variables_map parse_options(int argc, char *argv[])
{
        BPO::options_description general("General options");
        general.add_options()
                ("help,h",
                 "Produce help message")
                ("verbose,v",
                 "Emit debugging information");

        BPO::options_description actions("Actions (if none, then match)");
        actions.add_options()
                ("shell,s",
                 "Run interactive shell")
                ("edit,e",
                 "Edit record")
                ("passwd,p",
                 "Change password (no other options permitted)");

        BPO::options_description matching("Matching options");
        matching.add_options()
                ("match-key,m", BPO::value<vector_string>(),
                 "Restrict to records whose keys match")
                ("match-data-or-key,D", BPO::value<vector_string>(),
                 "Consider key and data for match (i.e., logical or)")
                ("match-data,d", BPO::value<vector_string>(),
                 "Restrict to records whose data match")
                ("exact-match,E",
                 "Exact match only (applies to all matching)");

        BPO::options_description display("Display options");
        display.add_options()
                ("keys-only,k",
                 "Display keys only")
                ("key-display,K",
                 "Display key even if unique match")
                ("grep,g", BPO::value<string>(),
                 "Output filter for data");

        BPO::options_description test("Test options (don't use outside regression tests)");
        test.add_options()
                ("TEST,T", BPO::value<string>(),
                 "Test mode, use local data directory, specify password on commandline");
        
        BPO::options_description options("Allowed options");
        options.add(general).add(actions).add(matching).add(display).add(test);
        
        BPO::positional_options_description pos;
        pos.add("match-key", -1);
        
        BPO::variables_map opt_map;
        BPO::store(BPO::command_line_parser(argc, argv).options(options).positional(pos).run(),
                   opt_map);
        BPO::notify(opt_map);

        if(opt_map.count("help")) {
                cout << options << endl;
                throw help_exception();
        }
        
        return opt_map;
}



static string get_password()
{
        cout << "Password: ";
        
        // Get pass phrase without echoing it
        termios before, after;
        tcgetattr(STDIN_FILENO, &before);
        after = before;
        after.c_lflag &= (~ICANON); // Disable canonical mode, including line buffering
        after.c_lflag &= (~ECHO);   // Don't echo characters
        tcsetattr(STDIN_FILENO, TCSANOW, &after);

        const int pass_len = 10240; // arbitrary, hopefully long enough
        char pass_phrase[pass_len];
        cin.getline(pass_phrase, pass_len);

        tcsetattr(STDIN_FILENO, TCSANOW, &before);
        cout << endl;

        // Iterate hash (arbitrarily) 50 times.  Motivated by gpg's behavior.
        string digest(pass_phrase);
        for(int i = 0; i < 50; i++)
                string digest = message_digest(digest, false);
        
        // Clear original pass phrase to minimize risk of seeing it in a swap or core image
        bzero(pass_phrase, sizeof(pass_len));
        
        return digest;
}



static void do_shell(const string password, const bool verbose)
{
        // ################
        cout << "do_shell() not yet implemented." << endl;
}


static void change_password(const string password, bool verbose)
{
        // ################
        cout << "change_password() not yet implemented." << endl;
}


static void do_edit(const string password,
                    const vector_string match_key,
                    const vector_string match_data,
                    const vector_string match_or,
                    const bool match_exact,
                    const bool verbose)
{
        
        
        // ################
        cout << "do_edit() not yet implemented." << endl;
        return;

        //vector_string;
}



static void do_match(const string password,
                     const vector_string match_key,
                     const vector_string match_data,
                     const vector_string match_or,
                     const bool match_exact,
                     const bool verbose)
{
        // ################
        cout << "do_match() not yet implemented." << endl;
}



