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
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>


#include "interface.h"
#include "crypt.h"
#include "leaf_proxy_map.h"
#include "mode.h"
#include "root.h"
#include "types.h"


namespace BPO = boost::program_options;
using namespace srd;
using namespace std;

class help_exception : public exception {};


static BPO::variables_map parse_options(int, char *[]);
static string get_password();
static void do_shell(const string);
static void change_password(const string);
static void do_edit(const string,
                    const vector_string,
                    const vector_string,
                    const vector_string,
                    bool);
static void do_match(const string,
                     const vector_string,
                     const vector_string,
                     const vector_string,
                     bool,
                     const bool keys_only,
                     const bool full_display,
                     const string grep);
static leaf_proxy_map get_leaf_proxy_map(root &root,
                                         const vector_string match_key,
                                         const vector_string match_data,
                                         const vector_string match_or,
                                         const bool match_exact);
static void user_add(root &root);
static bool user_edit(string &key, string &payload);


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
        mode(Testing, is_test);
        if(is_test)
                passwd = options["TEST"].as<string>();
        else
                passwd = get_password();
        
        // do something
        const bool verbose = options.count("verbose") > 0;
        mode(Verbose, verbose);

        if(options.count("shell")) {
                do_shell(passwd);
                return 0;
        }
        if(options.count("passwd")) {
                change_password(passwd);
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
                do_edit(passwd, match_key, match_data, match_or, match_exact);
                return 0;
        }

        string grep_pattern;
        if(options.count("grep") > 0)
                grep_pattern = options["grep"].as<string>();
        do_match(passwd, match_key, match_data, match_or, match_exact,
                 options.count("keys-only") > 0,
                 options.count("full-display") > 0,
                 grep_pattern);
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
                 "Exact key match");

        BPO::options_description display("Display options");
        display.add_options()
                ("keys-only,k",
                 "Display keys only (default true if multiple matches, false for single match)")
                ("full-display,f",
                 "Display full data (default true for single match, false for multiple matches)")
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



static void do_shell(const string password)
{
        // ################
        cout << "do_shell() not yet implemented." << endl;
}


static void change_password(const string password)
{
        // ################
        cout << "change_password() not yet implemented." << endl;
}


static void do_edit(const string password,
                    const vector_string match_key,
                    const vector_string match_payload,
                    const vector_string match_or,
                    const bool match_exact)
{
        root root(password, "");
        if(0 == match_key.size() && 0 == match_payload.size() && 0 == match_or.size()) {
                // Edit request with no search criteria means create new
                user_add(root);
                return;
        }
        leaf_proxy_map lpm = get_leaf_proxy_map(root, match_key, match_payload,
                                                match_or, match_exact);
        if(0 == lpm.size()) {
                cout << "No record matches." << endl;
                return;
        }
        if(1 == lpm.size()) {
                string proxy_key = lpm.begin()->first;
                leaf_proxy lp = lpm.begin()->second;
                string key = lp.key();
                string payload = lp.payload();
                if(!user_edit(key, payload))
                        return;
                root.set_leaf(proxy_key, key, payload);
                return;
        }
        
        for(leaf_proxy_map::iterator it = lpm.begin();
            it != lpm.end();
            ++it)
                it->second.print_key();

        return;
}



static void do_match(const string password,
                     const vector_string match_key,
                     const vector_string match_payload,
                     const vector_string match_or,
                     const bool match_exact,
                     const bool keys_only,
                     const bool full_display,
                     const string grep)
{
        root root(password, "");
        if(0 == match_key.size() && 0 == match_payload.size() && 0 == match_or.size()) {
                // Edit request with no search criteria means create new
                cerr << "No match criteria provided." << endl;
                cerr << "To match all records, use an empty key (\"\")." << endl;
                return;
        }
        leaf_proxy_map lpm = get_leaf_proxy_map(root, match_key, match_payload,
                                                match_or, match_exact);
        
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
        bool full_display_on = ((full_display && lpm.size() > 1)
                                || (!keys_only && 1 == lpm.size()));
        for(leaf_proxy_map::iterator it = lpm.begin();
            it != lpm.end();
            ++it) {
                it->second.print_key();
                if(full_display_on)
                        it->second.print_payload(grep);
        }
}



static leaf_proxy_map get_leaf_proxy_map(root &root,
                                         const vector_string match_key,
                                         const vector_string match_payload,
                                         const vector_string match_or,
                                         const bool match_exact)
{
        leaf_proxy_map lpm = root.filter_keys(match_key, match_exact);
        if(match_payload.size())
                lpm = lpm.filter_payloads(match_payload);
        if(match_or.size())
                lpm = lpm.filter_keys_or_payloads(match_or, match_or, match_exact);
        return lpm;
}


/*
  User interaction to add a new leaf to the root.
*/
static void user_add(root &root)
{
        string key("");
        string payload("");
        user_edit(key, payload);
        root.add_leaf(key, payload);
}



/*
  Call the user's $EDITOR on key/payload.
  Return true if modified, false otherwise.
*/
static bool user_edit(string &key, string &payload)
{
        ostringstream sdata;
        sdata << "[" << key << "]" << endl << payload;
        string data = sdata.str();

        ostringstream sfilename;
        sfilename << "srd-temp-" << getenv("LOGNAME") << "-" << getpid() << "-";
        sfilename << message_digest(key, true);
        file fdata(sfilename.str(), "/tmp");
        fdata.file_contents(data);

        // Probably should fork and exec to avoid shell layer in system()
        char *editor = getenv("EDITOR");
        if(!editor) {
                cerr << "EDITOR is not defined, we'll try vi." << endl;
                editor = (char *)"vi";
        }
        ostringstream scommand;
        scommand << editor << " " << fdata.full_path();
        int ret = system(scommand.str().c_str());
        if(ret) {
                perror("Failure executing editor");
                fdata.rm();
                return false;
        }

        string new_data = fdata.file_contents();
        fdata.rm();
        if(new_data == data)
                return false;
        size_t pos = new_data.find_first_of('\n');
        if(string::npos == pos) {
                cerr << "Failed to find end of key (end of first line)." << endl;
                return false;
        }
        string first_line(new_data.substr(0, pos));
        size_t first_length = first_line.size();
        if('[' != first_line[0] || ']' != first_line[first_length - 1]) {
                cerr << "First line is corrupt: failed to find square brackets at extremities."
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

