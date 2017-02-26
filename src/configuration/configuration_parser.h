#ifndef DOORMAT_CONFIGURATION_INITIALIZER_H
#define DOORMAT_CONFIGURATION_INITIALIZER_H

#include <string>
#include <fstream>
#include "../utils/json.hpp"
#include "configuration_maker.h"

class configuration_parser
{
public:
	configuration_parser(const std::string &path, bool verbose = false) 
		: path{path} , configuration_fd{}, verbose{verbose}, maker{verbose}
	{
		configuration_fd.open(path, std::ios::in);
		if( ! configuration_fd.is_open() )
		{
			notify("could not open file", path);
			throw std::invalid_argument("[PARSER] configuration file not found");
		}

		maker.set_tabs(tabs);
	};

	std::unique_ptr<configuration::configuration_wrapper> parse()
	{
		std::istream main_istream{&configuration_fd};
		parse_object(main_istream);
		return maker.get_configuration();
	}

	void parse_object(std::istream&input_stream, std::string current_path="")
	{
		if(current_path.empty()) current_path = path;
		std::string curline;
		std::getline(input_stream, curline);
		int current_line_number = 1;
		while(true)
		{
			//Tazio Ceri approves this piece of code (reluctantly), saying "non è la fine del mondo"
			try 
			{
				nlohmann::json::parse(curline, [this, current_line_number, &curline, &current_path](int depth, nlohmann::json::parse_event_t event, nlohmann::json& parsed)
				{
					if(parsed.is_object() && depth == 0)
					{
						if(parsed.cbegin().key() == "include" && parsed.cbegin().value().is_string())
						{
							notify("found include directory at line ", current_line_number, " of ", current_path, " parsing ", parsed.cbegin().value(), " recursively");
							std::string path = parsed.cbegin().value();
							std::filebuf included_fd;
							included_fd.open(path, std::ios::in);
							std::istream included_istream{&included_fd};
							increment_tabulation();
							parse_object(included_istream, path);
							decrement_tabulation();
						}
						else 
						{
							std::string obj_parsed = parsed.cbegin().key();
							notify("found valid object with key ", "\"", obj_parsed, "\" at line ",  current_line_number, " of ", current_path, "; starting validation");
							object_assign(parsed);
						}
						curline = "";
					}
					return true;
				});

				if(input_stream.eof()) 
				{
					if(curline.size())
						notify("parse finished, data currently readed and not yet parsed is ", 
							   curline, " please verify it is a valid json");
					break;
				}
				std::getline(input_stream, curline);
				++current_line_number;
			}
			catch ( const std::invalid_argument &einval )
			{
				std::string tmp;
				if ( input_stream.eof() )
				{
					if(curline.size())
						notify("parse finished, data currently readed and not yet parsed is ", 
							   curline, " please verify it is a valid json");
					break;
				}
				std::getline(input_stream, tmp);
				++current_line_number;
				curline += tmp;
			}
		}

		if(current_path == path) finished_successfully = true;
	}

	void object_assign(const nlohmann::json &json)
	{
		increment_tabulation();
		if(!maker.accept_setting(json))
		{
			std::string jsonrep = json;
			notify("unrecognized option", json);
		}
		decrement_tabulation();
	}


	bool parser_finished_successfully() const noexcept
	{
		return finished_successfully;
	}
	
	~configuration_parser() = default;

private:
	template<typename... Args>
	void notify(Args&&... args)
	{
		if(!verbose) return;
		std::cout << tabs << "[PARSER] " << utils::stringify(std::forward<Args>(args)...) << std::endl;
	}

	void increment_tabulation()
	{
		tabs += "\t";
	}

	void decrement_tabulation()
	{
		tabs = tabs.substr(0, tabs.size()-1);
	}

	const std::string path;
	std::filebuf configuration_fd;
	nlohmann::json current_json;
	bool verbose{false};
	bool finished_successfully{false};
	std::string tabs{};
	configuration::configuration_maker maker;
};


#endif //DOORMAT_CONFIGURATION_INITIALIZER_H
