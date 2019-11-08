#include <stdio.h>
#include <string>
#include <sqlite3.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <map>

#include "json.hpp"

using json = nlohmann::json;

int main(int argc, char **argv)
{
	/* We expect 4 arguments: the file path that contains all interesting
	 * system calls, the file path of sqlite database, and the file path
	 * of the generated function models from the previous step. 
	 * The system call file must have the following information per line:
	 * <name of the system call> <the name of the entry function>
	 * <locaton of the entry function of the call> */
	if (argc < 4) {
		std::cerr << "Usage:\n\t" 
			  << argv[0]
			  << " <QUEST FILE>\n"
			  << " <SQLITE DB> <MODEL INPUT>\n";
		std::cerr << "Each line in <QUEST FILE> must be in the form:\n"
			  << " <SYSCALL NAME> <ENTRY FUNCTION NAME> <ENTRY FUNCTION LOCATION>\n";
		return 1;
	}
	/* We give indicative names for the parse argv. */
	std::string quest_input = argv[1];
	std::string db_name 	= argv[2];
	std::string model_input = argv[3];
	
	/* Connect to the database. */
	sqlite3 *db;
	char *ErrMsg = 0;
	int rc;
	
	rc = sqlite3_open(db_name.c_str(), &db);
	if (rc) {
		std::cerr << "Cannot open SQLITE3 database: "
			  << sqlite3_errmsg(db);
		return 1;
	}

	
	/* Parse kernel function models. */
	std::map<std::string, std::map<std::string, json>> models;	/* file -> <funcs -> model> */
	std::ifstream model_file(model_input);
	std::string json_str;
	while (std::getline(model_file, json_str)) {
		json json_obj;
		try {
			json_obj = json::parse(json_str);
		} catch (json::exception &e) {
			std::cerr << "JSON parsing error: " << e.what() << "\n"
				  << "Exception ID: " << e.id << std::endl;
			continue;
		}
		
		/* meta data and file and func data must exist in the JSON object. */
		std::string file_name = json_obj["meta"]["file"].get<std::vector<std::string>>()[0];
		std::string func_name = json_obj["meta"]["func"].get<std::vector<std::string>>()[0];
		std::map<std::string, std::map<std::string, json>>::iterator it;
		it = models.find(file_name);
		if (it != models.end()) {
			it->second.insert( std::pair<std::string, json>(func_name, json_obj) );
		} else {
			std::map<std::string, json> funcs;
			funcs.insert( std::pair<std::string, json>(func_name, json_obj) );
			models.insert ( std::pair< std::string, std::map<std::string, json> >(file_name, funcs) );
		}
	}
	/* We can now safely close the model file. */
	model_file.close();

	/* We parse the quest file one quest (line) at a time. */
	std::ifstream quest_file(quest_input);
        std::string quest_str;
        while (std::getline(quest_file, quest_str)) {
		std::istringstream iss(quest_str);
		std::string syscall_name, func_name, file_name;
		if (!(iss >> syscall_name >> func_name >> file_name)) {
			std::cerr << "Quest line not well-formed (skipped): " << quest_str << std::endl;
			continue;
		}
		std::cout << "Syscall: " << syscall_name << "\tLoc: "
			  << file_name << "/" << func_name << std::endl;
		
		/* Find the JSON object. */
		std::map<std::string, std::map<std::string, json>>::iterator it;
		it = models.find(file_name);
		if (it == models.end()) {
			std::cerr << "File '" << file_name << "' is not in the database.\n"
				  << "Double check your spelling or something else went wrong!" << std::endl;
			continue;
		} else {
			std::map<std::string, json>::iterator inner_it;
			inner_it = it->second.find(func_name);
			if (inner_it == it->second.end()) {
				std::cerr << "Function '" << func_name << "' is not in the database.\n"
					  << "We attempted to find it in '" << file_name << "'.\n"
					  << "Double check your spelling or something else went wrong!" << std::endl;
				continue;
			} else {
				/* We obtain the JSON object of the system call. */
				json syscall_json_obj = inner_it->second;
				
				//TODO: CODE HERE TO BUILD THE FULL VERSION OF SYSCALL MODEL.
			}
		}
	}	

	/* We are now. Now close the quest file. */
	quest_file.close();	
	/* Close the database at the very end. */
	sqlite3_close(db);
}
