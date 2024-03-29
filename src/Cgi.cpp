/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Cgi.cpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hrings <hrings@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/03/23 13:56:47 by hrings            #+#    #+#             */
/*   Updated: 2023/04/21 18:47:42 by hrings           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Cgi.hpp"

Cgi::Cgi(http::Server* server) : error_code(NONE)
{
	this->server = server;
	char *cwd = getcwd(NULL, 0);
  	if (!cwd)
    {
		this->error_code = INTERNALSERVERERROR;
		return ;	
	}
  	this->working_dir = cwd;
  	free(cwd);

}
Cgi::~Cgi()
{
	if(this->env)
	{
		for(int i = 0; env[i]; ++i)
			free(env[i]);
		free(env);
		
	}
	if (argv[0])
		free(argv[0]);
	if (argv[1])
		free(argv[1]);
}

ErrorCode Cgi::getErrorCode()
{
	return (this->error_code);
}

void Cgi::set_request(Request *request)
{
	this->_request = request;
}

void Cgi::run_cgi()
{
	methodCheck();
	if(!this->error_code)
		initEnv();
	if(!this->error_code)
		executeScript();
}
void Cgi::methodCheck()
{
	if (!(this->_request->readMethod() == GET || this->_request->readMethod() == POST))
		this->error_code = METHODNOTALLOWED;
}
void Cgi::initEnv()
{
	
	char *cwd = getcwd(NULL, 0);
  	if (!cwd)
    	return ;
  	std::string cwd_ = cwd;
  	free(cwd);
	this->file_path = http::remove_extra_backslash(cwd_ + "/" + this->server->readRoot() + "/cgi-bin/" + _request->getCgi_exe());
	if (this->_request->readMethod() == GET)
		this->env_var["REQUEST_METHOD"] = "GET";
	if (this->_request->readMethod() == POST)
	{
		this->env_var["CONTENT_LENGTH"] = this->_request->headers["content-length"];
		this->env_var["CONTENT_TYPE"] = this->_request->headers["content-type"];
		this->env_var["REQUEST_METHOD"] = "POST";
	}
	this->env_var["SERVER_NAME"] = this->server->getListen().front().ip;
	this->env_var["SERVER_PORT"] = http::int_to_str(this->server->getListen().front().port);
	this->env_var["GATEWAY_INTERFACE"] = "CGI/1.1";
	this->env_var["SERVER_PROTOCOL"] = "HTTP/1.1";
	this->env_var["PATH_INFO"] = cwd_ + this->_request->readPath();
	this->env_var["PATH_TRANSLATED"] = cwd_ + this->_request->readPath();
	this->env_var["REQUEST_URI"] = this->_request->readPath();
	this->env_var["REMOTE_ADDR"] = this->_request->read_ip_Addr();
	this->env_var["SCRIPT_NAME"] = _request->getCgi_exe();
	this->env_var["QUERY_STRING"] = this->_request->readQuery();
	this->env_var["REQUEST_URI"] = cwd_ + this->_request->readPath();
	this->env_var["SERVER_SOFTWARE"] = "WEBSERV/1.0";
	for (std::map<std::string, std::string>::iterator it = this->_request->headers.begin();
		it != this->_request->headers.end();
		++it)
	{
			std::string header = "HTTP_" + http::to_upper_case(it->first);
			std::replace(header.begin(), header.end(), '-', '_');
			this->env_var[header] = it->second;
	}
	this->env = (char **)calloc(sizeof(char *), this->env_var.size() + 1);
	if (!this->env)
	{
		this->error_code = INTERNALSERVERERROR;
		return ;
	}
	std::map<std::string, std::string>::iterator it = this->env_var.begin();
	for (int i = 0; it != this->env_var.end(); ++it, ++i)
	{
		std::string tmp = it->first + "=" +it->second;
		env[i] = strdup(tmp.c_str());
	}
	size_t index = this->_request->readPath().find_last_of("/");
  	argv[0] = strdup(this->file_path.c_str());
  	argv[1] = strdup(_request->getCgi_exe().c_str());
	argv[2] = NULL;
	this->execute_dir = this->working_dir + this->_request->readPath().substr(0, index);
}

void Cgi::executeScript()
{
	if (this->_request->readMethod() == GET && this->_request->getCgi_method() == "GET")
		runGetScript();
	else if (this->_request->readMethod() == POST && this->_request->getCgi_method() == "POST")
		runPostScript();
	else
		this->error_code = METHODNOTALLOWED;

}

void Cgi::runGetScript()
{
	pid_t pid = fork();
	if (pid== 0)
	{
		if (chdir(this->execute_dir.c_str()) == -1)
			exit(INTERNALSERVERERROR);
		std::string tmp_file(this->working_dir + CGITMPFOLDER);
		int tmp_fd;
		mode_t	mode;
		mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
		tmp_fd = open(tmp_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, mode);
		if (tmp_fd < 0)
		{
			http::print_status(ft_RED, "open file error write");
			exit(1);
		}
		if(dup2(tmp_fd,1) == -1)
			exit(INTERNALSERVERERROR);
		execve(this->argv[0], this->argv, env);
		exit(INTERNALSERVERERROR);
	}
	else
	{
		time_t start = time(NULL);
		int status;
		int wait_return;
		do{
			wait_return = waitpid(pid, &status, WNOHANG);
			if (wait_return == -1)
			{
				http::print_status(ft_RED, "waitpid error");
			}
			if (wait_return)
			{
				break ;
			}
			usleep(500);
		}while(difftime(time(NULL), start) < TIMEOUTTIME);
		if (WEXITSTATUS(status)) {
			http::print_status(ft_RED, "execusion of cgi script failed");
			this->error_code = INTERNALSERVERERROR;
			return ;
        }
		std::string tmp_file(this->working_dir + "/cgi-bin/tmp");
		std::ifstream		output;
		std::ostringstream	buff_tmp;
		output.open(tmp_file.c_str());
		if (output.good() )
		{
			buff_tmp.str("");
			buff_tmp << output.rdbuf();
			output.close();
			std::remove(tmp_file.c_str());
			this->_request->getRequestBody().insert(0, buff_tmp.str());		
		}
		else
		{
			http::print_status(ft_RED, "open file error read");
			return ;
		}
		this->error_code = OK;
	}
}
		
void Cgi::runPostScript()
{
	int pip[2];
	if (pipe(pip) == -1)
	{
		this->error_code = INTERNALSERVERERROR;
		return ;
	}
	pid_t pid = fork();
	if (pid== 0)
	{

		std::string tmp_file(this->working_dir + CGITMPFOLDER);
		int tmp_fd;
		mode_t	mode;
		mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
		tmp_fd = open(tmp_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, mode);
		if (tmp_fd < 0)
		{
			http::print_status(ft_RED, "open file error write");
			exit(1);
		}
		close(pip[1]);
		dup2(pip[0], STDIN_FILENO);
		dup2(tmp_fd, STDOUT_FILENO);
		execve(this->file_path.c_str(), this->argv, env);
		exit(1);
	}
	else
	{
		close(pip[0]);
		if (this->_request->getRequestBody().length())
		{
			if (write(pip[1], this->_request->getRequestBody().c_str(), this->_request->getRequestBody().length()) <= 0)
			{
				this->error_code = INTERNALSERVERERROR;
				return ;
			}
		}
		close(pip[1]);
		this->_request->getRequestBody().clear();
		time_t start = time(NULL);
		int status;
		int wait_return;
		do{
			wait_return = waitpid(pid, &status, WNOHANG);
			if (wait_return == -1)
			{
				http::print_status(ft_RED, "waitpid error");
			}
			if (wait_return)
			{
				break ;
			}
		}while(difftime(time(NULL), start) < 10);
		if (WEXITSTATUS(status)) {
			http::print_status(ft_RED, "execusion of cgi script failed");
			this->error_code = INTERNALSERVERERROR;
			return ;
        }
		std::string tmp_file(this->working_dir + CGITMPFOLDER);
		std::ifstream		output;
		std::ostringstream	buff_tmp;
		output.open(tmp_file.c_str());
		if (output.good() )
		{
			buff_tmp.str("");
			buff_tmp << output.rdbuf();
			output.close();
			std::remove(tmp_file.c_str());
			this->_request->getRequestBody().insert(0, buff_tmp.str());		
		}
		else
		{
			http::print_status(ft_RED, "open file error read");
			return ;
		}
		this->error_code = OK;
	}
}

void Cgi::parse_body_for_headers()
{
	size_t pos;
	size_t end = this->_request->getRequestBody().find(EOL);
	std::string key;
	std::string value;
	
	while ( end != std::string::npos)
	{
		if (this->_request->getRequestBody().find(EOL) == 0)
		{
			this->_request->getRequestBody().erase(0, end + 2);
			break;
		}
		if ((pos = this->_request->getRequestBody().find(':', 0)) != std::string::npos)
		{
			key = this->_request->getRequestBody().substr(0, pos);
			value = http::trim_whitespace(this->_request->getRequestBody().substr(pos + 1, end - pos -1));
			this->_request->headers[http::to_lower_case(key)] = value;
		}
		this->_request->getRequestBody().erase(0, end + 2);
		end = this->_request->getRequestBody().find(EOL);
	}
}
