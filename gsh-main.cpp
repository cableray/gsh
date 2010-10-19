#include <cstdlib>
#include <dirent.h>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>                      // For istringstream
#include <sys/errno.h>
#include <sys/param.h>          // For MAXPATHLEN
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h> 
#include <unistd.h>
#include <map>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>

#include <readline/readline.h>
#include <readline/history.h>
#define SHELL_NAME "gsh"

using namespace std;
extern char **environ;

struct job 
{
  string name;
  int PID;
  bool isBackground;
};

struct environment 
{
  vector<job> jobs;
  map<string, string> localvar;
};

typedef int (*command)(std::vector<std::string>);

vector<string> LEGAL_COMMANDS;
bool DEBUG = false;

int com_ls(vector<string>);
int com_cd(vector<string>);
int com_alias(vector<string>);
int com_unalias(vector<string>);
int com_echo(vector<string>);
int com_exit(vector<string>);
int com_pwd(vector<string>);


// Duplicate a C-style string for Readline
// From: http://cnswww.cns.cwru.edu/~chet/readline/readline.html#SEC23
char * dupstr (const char *s) {
    char *r;

    r = (char *) malloc (strlen (s) + 1);
    strcpy (r, s);
    return (r);
}


// From: http://cnswww.cns.cwru.edu/~chet/readline/readline.html#SEC23
/* A static variable for holding the line. */
static char *rl_line_read = (char *)NULL;

// From http://cnswww.cns.cwru.edu/~chet/readline/readline.html#SEC23
/* Read a string, and return a pointer to it.
 *    Returns NULL on EOF. */
char * rl_gets(const char* prompt) {
    /* If the buffer has already been allocated,
     *      return the memory to the free pool. */
    if (rl_line_read) {
        free(rl_line_read);
        rl_line_read = (char *)NULL;
    }

    /* Get a line from the user. */
    rl_line_read = readline(prompt);

    /* If the line has any text in it,
     *      save it on the history. */
    if (rl_line_read && *rl_line_read)
        add_history(rl_line_read);

    return (rl_line_read);
}

// Transform a C-style string into a C++ vector of string tokens.  The C-style
// string is tokenized on whitespace.
vector<string> tokenize(char *line) {
  vector<string> tokens;
  vector<string> tempToken;
  // Convert C-style string to C++ string object
  string lineStr(line);
  // istringstream allows us to treat the string like a stream
  istringstream ist(lineStr);
  string tokenStr;
  unsigned int i=0;
  unsigned int j=0;
  int count=0;
  bool reachedfirstQ = false;
  string::size_type nextSQ=string::npos;
  string::size_type SQ=string::npos;
  string tempString="";

  while (ist >> tokenStr) 
    tokens.push_back(tokenStr);
  for(unsigned int p=0; p<tokens.size(); p++) // count the number of single quotes
  {
    if(tokens[p].find('"') != string::npos || tokens[p].find('`') != string::npos)
    {
      cout << SHELL_NAME << ": double quotes and backticks not allowed." << endl;
      tokens.clear();
      return tokens;
    }
  }

  for(unsigned int m=0; m<tokens.size(); m++) // count the number of single quotes
  {
    for(unsigned int n=0; n<tokens[m].size(); n++)
    {
      if(tokens[m].at(n) == '\'')
      {
        count++;
        //cout << "SQ at " << n << " in token " << m << " count is at: " << count <<endl;
      }
    }
  }
  if(count%2 == 0) //make sure the number of single quotes is an even number else clear the token vector
  {
    for(i=0; i<tokens.size();i++) 
    {
      SQ = tokens[i].find("'");  // find the first quote
      if(i == j && nextSQ != string::npos)
        SQ = tokens[i].find("'", (nextSQ+1));
      //cout << "SQ at " << SQ << " in token " << i << endl;
      if (SQ != string::npos)
      {
        reachedfirstQ=true;
        nextSQ = string::npos;
        j = i; // start looking from after where the first quote was found
        while(nextSQ == string::npos)
        {
          nextSQ = tokens[j].find("'"); // find the second quote
          if(i == j && SQ >= nextSQ)
            nextSQ = tokens[j].find("'", (SQ+1));
          if(nextSQ != string::npos)
          {
            //cout << "nextSQ at " << nextSQ << " in token " << j << endl;
            break; // break out if second quote is found to preserve correct value of j
          } 
          j++;
        }
      }
      if (!reachedfirstQ)
        tempToken.push_back(tokens[i]);
      
      tempString="";
      if(SQ != string::npos && nextSQ != string::npos)
      {
        //cout << "SQ at " << SQ << " in token " << i << ". nextSQ at " << nextSQ << " in token " << j << endl;
        
        for(unsigned int k=i; k<=j; k++)
        {
          if(k == i && k == j)
          {
            //cout << "testbeg&end" << endl;
            tempString+=(tokens[i].substr(SQ+1, nextSQ-1));
          }
          else if(k == i)
          {
            //cout << "testbeg" << endl;
            tempString+=(tokens[i].substr(SQ+1));
          }
          else if(k == j)
          {
            //cout << "testend" << endl;
            tempString+=" " + (tokens[k].substr(0, nextSQ));
          }
          else
            tempString+=" " + (tokens[k]);
        }
        tempToken.push_back(tempString);
        //cout << "tempToken.size()" << tempToken.size() << endl;
          for(unsigned int h=0; h<tempToken.size(); h++)
            cout << tempToken[h] << " . ";
          cout << endl;
        //cout << "test" << endl;
      }
    }
  }
  else
  {
    cout << SHELL_NAME << ": missing closing single quote" << endl;
    tokens.clear();
    return tokens;
  }

  
  //for(unsigned int h=0; h<tokens.size(); h++)
    //cout << tempToken[h] << " . ";
  //cout << endl;

  
  return tempToken;
}


//convert an object or base type to a string using stringstream
template <class T>
inline std::string to_string (const T& t)
{
  std::stringstream ss;
  ss << t;
  return ss.str();
}


char * environment_generator (const char *text, int state) {
    // Convert the entered text to a C++ string
    string textStr(text);

    if (DEBUG) {
        cout << endl << "textStr = " << textStr << endl;
        cout << endl << "rl_line_buffer = " << rl_line_buffer << endl;
    }

    // this is where we hold all the matches
    // Must be static because this function is called repeatedly
    static vector<string> matches;

    // If this is the first time called, init the index and build the
    // vector of all possible matches
    if (state == 0) {
        char* variable = environ[0];
        for (int i = 0; variable != (char *)NULL;) {
            if (DEBUG) {
                cout << "environ[" << i << "] = " << environ[i] << endl;
            }
            string variableStr(variable);
            variableStr = "$" + variableStr.substr(0, variableStr.find("="));
            if (variableStr.find(textStr) == 0) {
                if (DEBUG) {
                    cout <<  endl << "variableStr = " << variableStr << endl;
                }
                matches.push_back(variableStr);
            }
            variable = environ[++i];
        }
    }

    // Return one of the matches, or NULL if there are no more
    if (matches.size() > 0) {
        const char * match = matches.back().c_str();
        // delete the last element
        matches.pop_back();
        return dupstr(match);
    } else {
        return ((char *)NULL);
    }
}

// Warning: This function doesn't work in the alamode lab anymore because the
// file system isn't setting dp->d_type.  Instead, it's always zero.
char * directory_generator (const char *text, int state) {
    // Convert the entered text to a C++ string
    string textStr(text);

    if (DEBUG) {
        cout << endl << "textStr = " << textStr << endl;
        cout << endl << "rl_line_buffer = " << rl_line_buffer << endl;
    }

    // this is where we hold all the matches
    // Must be static because this function is called repeatedly
    static vector<string> matches;

    // If this is the first time called, init the index and build the
    // vector of all possible matches
    if (state == 0) {
        char path[MAXPATHLEN];
        getcwd(path, MAXPATHLEN);
        DIR *dirp = opendir(path);
        struct dirent *dp;
        while ((dp = readdir(dirp)) != NULL) {
            if (dp->d_type == DT_DIR) {
                string dirStr(dp->d_name);
                if (DEBUG) {
                    cout << endl << "dp->d_name = " << dp->d_name << endl;
                }
                if (dirStr.find(textStr) == 0) {
                    if (DEBUG) {
                        cout <<  endl << "dirStr = " << dirStr << endl;
                    }
                    matches.push_back(dirStr + "/");
                }                
            }
        }
        closedir(dirp);
    }

    // Return one of the matches, or NULL if there are no more
    if (matches.size() > 0) {
        const char * match = matches.back().c_str();
        // delete the last element
        matches.pop_back();
        return dupstr(match);
    } else {
        return ((char *)NULL);
    }
}

/* Generator function for command completion.  STATE lets us
 * know whether to start from scratch; without any state
 * (i.e. STATE == 0), then we start at the top of the list. */
char * command_generator (const char *text, int state) {
    // Convert the entered text to a C++ string
    string textStr(text);

    // This is where we hold all the matches
    // Must be static because this function is called repeatedly
    static vector<string> matches;

    // If this is the first time called, init the index and build the
    // vector of all possible matches
    if (state == 0) {
        for (unsigned int i = 0; i < LEGAL_COMMANDS.size(); i++) {
            // If the text entered matches one of our commands
            if (LEGAL_COMMANDS[i].find(textStr) == 0) {
                matches.push_back(LEGAL_COMMANDS[i]);
            }
        }
	  const char * one_path = "/usr/local/bin";
        DIR *dirp = opendir(one_path);
        struct dirent *dp;
        while ((dp = readdir(dirp)) != NULL) {
	  string dirStr(dp->d_name);
	  if (dirStr.find(textStr) == 0) {
	    matches.push_back(dirStr);
	  }
	}
    }

    // Return one of the matches, or NULL if there are no more
    if (matches.size() > 0) {
        const char * match = matches.back().c_str();
        // delete the last element
        matches.pop_back();
        return dupstr(match);
    } else {
        return ((char *)NULL);
    }
}

// Adapted from: http://cnswww.cns.cwru.edu/~chet/readline/readline.html#SEC23
// Try to complete with a command, directory, or variable name;
// otherwise, use filename completion 
char ** command_completion (const char *text, int start, int end) {
    char **matches;

    // The whole line entered so far
    string lineStr(rl_line_buffer);

    if (DEBUG) {
      cout << endl << "text = " << text << " start = " << start << " end = " << end << endl;
    }

    matches = (char **)NULL;

    /* If this word is at the start of the line, then it is a command
     * to complete. */ 
    if (start == 0) {
        rl_completion_append_character = ' ';
        matches = rl_completion_matches(text, command_generator);
    } else if (lineStr.find("cd ") == 0) {
        rl_completion_append_character = '\0';
        matches = rl_completion_matches(text, directory_generator);
    } else if (lineStr.find("echo $") == 0) {
        rl_completion_append_character = ' ';
        matches = rl_completion_matches(text, environment_generator);
    }
    return (matches);
}

// Adapted from: http://cnswww.cns.cwru.edu/~chet/readline/readline.html#SEC23
void initialize_readline() {

    // Set up our legal commands
    LEGAL_COMMANDS.push_back("cd");
    LEGAL_COMMANDS.push_back("ls");
    LEGAL_COMMANDS.push_back("alias");
    LEGAL_COMMANDS.push_back("unalias");
    LEGAL_COMMANDS.push_back("echo");
    LEGAL_COMMANDS.push_back("exit");
    LEGAL_COMMANDS.push_back("jobs");
    LEGAL_COMMANDS.push_back("pwd");

    // Took out the '$' out of the default so that it is part of a word
    rl_basic_word_break_characters = " \t\n\"\\'`@><=;|&{(";

    // Tell the completer that we want a crack first.
    rl_attempted_completion_function = command_completion;
}


int com_ls(vector<string> tokens)
{
	return 0;
}


int com_cd(vector<string> tokens) 
{
	return chdir(tokens[1].c_str());
}

string pwd()
{
  return string(get_current_dir_name());
}

int com_pwd(vector<string> tokens)
{
  cout << pwd() << endl;
	return 0;
}

int com_alias(vector<string> tokens)
{
	return 0;
}

int com_unalias(vector<string> tokens) 
{
	return 0;
}

// will echo out any tokens
int com_echo(vector<string> tokens) 
{
  for(int i(1);i<tokens.size();i++){
    if (i>1) cout<<" ";
    cout<<tokens[i];
  }
  cout<<endl;
	return 0;
}


// exits
int com_exit(vector<string> tokens)
{
  cout << SHELL_NAME << ": logged out." << endl ;
  exit(0);
	return 0;
}

//handles: external commands
//         redirects
//         pipes
int dispatchCmd(vector<string> tokens)
{
	return 0;
}

//decides if the line is a local command or not.
int executeLine(vector<string> tokens, map<string, command> functions, environment& env)
{
  if(tokens.size() != 0)
  {
    int returnValue=0;

    map<string,command>::iterator lookup = functions.find(tokens[0]);
    if (lookup == functions.end())
      returnValue = dispatchCmd(tokens);
    else
      returnValue = ((*lookup->second)(tokens));
  
    return returnValue;
  }
  else
    return 0;
}

//does the checking of the return value to update the emoticon. Also grabs the current working dir
char* prompt(int returnValue)
{
  string prompt=pwd()+" "+to_string(returnValue)+"$ ";
  return rl_gets(prompt.c_str());
}


// Finds and does Lookup on any tokens that are vars
void varSub(vector<string>& tokens, environment& env)
{
if(tokens.size() != 0){
  for(unsigned int i = 0; i < tokens.size(); i++) 
  {
    if (tokens[i].at(0) == '$')
    { 
      if (getenv(tokens[i].substr(1).c_str()) != NULL )
        tokens[i] = getenv(tokens[i].substr(1).c_str());
      else if ( env.localvar.find(tokens[i].substr(1)) != env.localvar.end() )
        tokens[i] = env.localvar.find(tokens[i].substr(1))->second;
      else
        tokens[i] = "";
    }
  }
}
}


// handles two things: "VAR=value" and then "VAR=value <command>"
void localVarHandling(vector<string>& tokens, environment& env)
{
if(tokens.size() != 0){
  if ((tokens.size() == 1) && (tokens[0].find("=") != string::npos)) // local var assingment
  {
    string::size_type equals = tokens[0].find("=");
    string var = tokens[0].substr(0, equals);
    string value = tokens[0].substr(equals+1);
    env.localvar[var] = value;
  }
  else if ((tokens.size() >= 2) && (tokens[0].find('=') != string::npos)) // local var assignment + command
  {
    string::size_type equals = tokens[0].find('=');
    string var = tokens[0].substr(0, equals);
    string value = tokens[0].substr(equals+1);
    env.localvar[var] = value;
    tokens.erase(tokens.begin());
  }
}
}

//does expasion on {1,2,3,4} syntax
void tokenXpand(vector<string>& tokens, environment& env)
{
if(tokens.size() != 0){
  vector<string> subTokens;
  for(unsigned int i=1; i<tokens.size();i++)
  {
    string::size_type openBrace = tokens[i].find('{');
    string::size_type closeBrace = tokens[i].find('}');
    if ((openBrace != string::npos) && (closeBrace != string::npos))
    {
      subTokens.push_back(tokens[i].substr(0, openBrace));
      string::size_type comma = tokens[i].find(',');
      subTokens.push_back(tokens[i].substr(openBrace+1, (comma-openBrace-1)));
      //cout << "comma at: " << comma << " ";
      while(comma != closeBrace)
      {
        string::size_type nextComma = tokens[i].find(',',comma+1); // finds next comma after the last one
        if(nextComma == string::npos) // if next comma doesn't exist (we're at the end of the list, set it to closeBrace
          nextComma = closeBrace;
        //cout << nextComma << " ";
        subTokens.push_back(tokens[i].substr(comma+1, (nextComma-comma-1)));
        comma = nextComma;
      }      
      for(unsigned int j=1; j<subTokens.size(); j++)
        tokens.insert(tokens.begin()+(i+j),(subTokens[0]+subTokens[j])); //insert the newly expanded into tokens
      tokens.erase(tokens.begin()+i); // remove the shorthand curly brace syntax token
    }
  }
}
}

int main() {
  map<string, command> functions;
  vector<string> tokens;
  vector<string> secondTokens;
  environment env;
  char* line;
  int returnValue=0;
  //vector<string>::iterator tk = tokens.begin();
  
  functions["echo"] = &com_echo;
  functions["exit"] = &com_exit;
  functions["alias"] = &com_alias;
  functions["unalias"] = &com_unalias;
  functions["pwd"] = &com_pwd;
  functions["ls"] = &com_ls;
  functions["cd"] = &com_cd;

  initialize_readline();       // Bind our completer. 

  while (true) 
  {
    line = prompt(returnValue);
    if (!line) 
      break;
    // Break the line into tokens
    tokens = tokenize(line);
    tokenXpand(tokens, env);
    localVarHandling(tokens, env); // handles two things: "VAR=value" and then "VAR=value <command>"
    varSub(tokens, env); // substitutues $VAR into value

    returnValue = executeLine(tokens, functions, env);
  }

  exit(0);
}
