﻿#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <list>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <sstream>

#include <winsock2.h>
#pragma comment(lib, "Ws2_32.lib")

class work_item
{
public:
	std::vector<char>* header;
	std::string target;
	std::string method;

	~work_item(void)
	{
		//delete header;
	}
};

template <typename T> class blocking_queue
{
public:

	T pop()
	{
		T item = NULL;
		std::unique_lock<std::mutex> mlock(mutex);
		while (queue.empty() && !completed)
		{
			condition_variable.wait(mlock);
		}

		mtx.lock();
		if (!queue.empty())
		{
			item = queue.front();
			queue.pop();
		}

		mtx.unlock();
		return item;
	}

	/*void pop(T& item)
	{
		std::unique_lock<std::mutex> mlock(mutex_);
		while (queue_.empty())
		{
			cond_.wait(mlock);
		}

		item = queue_.front();
		queue_.pop();
	}*/

	void push(T& item)
	{
		std::unique_lock<std::mutex> mlock(mutex);
		queue.push(item);
		mlock.unlock();
		condition_variable.notify_one();
	}

	/*void push(T&& item)
	{
		std::unique_lock<std::mutex> mlock(mutex_);
		queue_.push(std::move(item));
		mlock.unlock();
		cond_.notify_one();
	}*/

	void finalise()
	{
		completed = true;
		condition_variable.notify_all();
	}

private:
	std::queue<T> queue;
	std::mutex mutex;
	std::mutex mtx;
	std::condition_variable condition_variable;
	bool completed;
};

class boyer_moore
{
private:
	static const int ALPHABET_SIZE = 256;
	std::vector<char> pattern;
	int last[ALPHABET_SIZE];
	std::vector<int> match;
	std::vector<int> suffix;

public:
	boyer_moore(std::vector<char> pattern2) : match(pattern2.size()), suffix(pattern2.size())
	{
		pattern = pattern2;
		ComputeLast();
		ComputeMatch();
	}

	/*int index_of(std::vector<char> text)
	{
		int i = pattern.size() - 1;
		int j = pattern.size() - 1;
		while (i < text.size())
		{
			if (pattern[j] == text[i])
			{
				if (j == 0)
				{
					return i;
				}
				j--;
				i--;
			}
			else
			{
				i += pattern.size() - j - 1 + max((j - last[text[i]]), match[j]);
				j = pattern.size() - 1;
			}
		}
		return -1;
	}*/

	int index_of(std::vector<char> prefix, int index1, char* text, int length)
	{
		int i = pattern.size() - 1;
		int j = pattern.size() - 1;
		while (i < prefix.size() - index1 + length)
		{
			char _byte = i < prefix.size() - index1 ? prefix[i + index1] : text[i - (prefix.size() - index1)];
			if (pattern[j] == _byte)
			{
				if (j == 0)
				{
					return i + index1;
				}
				j--;
				i--;
			}
			else
			{
				i += pattern.size() - j - 1 + max(j - last[_byte], match[j]);
				j = pattern.size() - 1;
			}
		}

		return -1;
	}

private:
	void ComputeLast()
	{
		for (int k = 0; k < ALPHABET_SIZE; k++)
		{
			last[k] = -1;
		}

		for (int j = pattern.size() - 1; j >= 0; j--)
		{
			if (last[pattern[j]] < 0)
			{
				last[pattern[j]] = j;
			}
		}
	}

	void ComputeMatch()
	{
		for (int j = 0; j < match.size(); j++)
		{
			match[j] = match.size();
		}

		ComputeSuffix();
		for (int i = 0; i < match.size() - 1; i++)
		{
			int j = suffix[i + 1] - 1;
			if (suffix[i] > j)
			{
				match[j] = j - i;
			}
			else
			{
				//match[j] = Math.Min(j - i + match[i], match[j]);
			}
		}

		if (suffix[0] < pattern.size())
		{
			for (int j = suffix[0] - 1; j >= 0; j--)
			{
				if (suffix[0] < match[j]) { match[j] = suffix[0]; }
			}
			{
				int j = suffix[0];
				for (int k = suffix[j]; k < pattern.size(); k = suffix[k])
				{
					while (j < k)
					{
						if (match[j] > k)
						{
							match[j] = k;
						}
						j++;
					}
				}
			}
		}
	}

	void ComputeSuffix()
	{
		suffix[suffix.size() - 1] = suffix.size();
		int j = suffix.size() - 1;
		for (int i = suffix.size() - 2; i >= 0; i--)
		{
			while (j < suffix.size() - 1 && pattern[j] != pattern[i])
			{
				j = suffix[j + 1] - 1;
			}
			if (pattern[j] == pattern[i])
			{
				j--;
			}
			suffix[i] = j + 1;
		}
	}
};

class socket_server
{
public:
	const std::vector<char> delimiter = { '\r', '\n','\r','\n' };
	static const int size = 102;
	boyer_moore* boyer_moore2;

	void handle_requests(int client_socket, blocking_queue<work_item*>* processing_queue)
	{
		int  count;
		char buffer[size];
		std::vector<char> byteBag;
		do
		{
			//memset(buffer, 0, size);
			count = recv(client_socket, buffer, size, 0);
			if (count > 0)
			{
				int offset = byteBag.size() < delimiter.size() ? byteBag.size() : byteBag.size() - delimiter.size() + 1;
				int index = boyer_moore2->index_of(byteBag, offset, buffer, count);
				if (index == -1)
				{
					for (int i = 0; i < count; i++)
					{
						byteBag.push_back(buffer[i]);
					}
				}
				else
				{
					std::vector<char> bytes;
					if (index > byteBag.size())
					{
						for (int i = 0; i < byteBag.size(); i++)
						{
							bytes.push_back(byteBag[i]);
						}

						for (int i = byteBag.size(); i < index - byteBag.size(); i++)
						{
							bytes.push_back(buffer[i]);
						}
					}
					else
					{
						for (int i = 0; i < index; i++)
						{
							bytes.push_back(byteBag[i]);
						}
					}

					byteBag.clear();
					auto l = new work_item();
					auto header_string = std::string((bytes).begin(), (bytes).end());
					std::vector<std::string>* segments = split(header_string);
					if ((*segments).size()>0)
					{	
						auto command_line = (*segments)[0];
						auto command_segments = split(command_line,' ');
						if ((*command_segments).size() == 3)
						{
							(*l).method = (*command_segments)[0];
							(*l).target = (*command_segments)[1];
						}

						for (auto& segment : *segments)
						{
						}

						delete command_segments;
						delete segments;
					}

					l->header = &bytes;
					processing_queue->push(l);
				}
			}
		} while (count > 0);
		processing_queue->finalise();
	}

	void process_requests(int client_socket, blocking_queue<work_item*>* processing_queue)
	{
		work_item* work_item;
		do
		{
			work_item = processing_queue->pop();
			if (work_item != NULL)
			{
				print(work_item->header);
				if ((*work_item).target == "/")
				{
					std::string html;
					html.append("<!DOCTYPE html><html><body>hello <img src='/s1.jpg'/>");
					html.append("hello <form method='POST' enctype='multipart/form-data'><input type='text' value='jjj++++++' name='firstname'/><button type='submit'>submit</button>");
					html.append("<input type='file' name='fileToUpload' id='fileToUpload'>");
					html.append("</form>hello <a href='/sys.php'>sys.php</a></body></html>");
					std::string header = get_response_header(html.length());
					send(client_socket, &header[0], header.length(), 0);
					send(client_socket, &html[0], html.length(), 0);					
					//int count = send(client_socket, "200", 18, 0);
				}

				//int count = send(client_socket, "200", 18, 0);
				delete work_item;
				shutdown(client_socket, 0);
			}

		} while (work_item != NULL);

		delete processing_queue;
		closesocket(client_socket);
		printf("socket closed\r\n");
	}

	std::string get_response_header(int content_length)
	{
		std::string response_header;
		response_header.append("HTTP/1.1 200 OK\r\n");
		if (content_length != 0)
		{
			response_header.append("Content-Length:");
			response_header.append(std::to_string(content_length));
			response_header.append("\r\n");
		}

		//responseHeader.AppendLine(string.Format("Connection:{0}", "Close"));          

		std::string content_type = "Content-Type: text/html;charset=utf-8\r\n";
		response_header.append(content_type);
		response_header.append("\r\n");
		return response_header;
	}

	void accept_requests(int server_socket)
	{
		struct sockaddr_in client_socket_address;
		int length = sizeof(client_socket_address);
		while (true)
		{
			int client_socket = accept(server_socket, (struct sockaddr *)&client_socket_address, &length);
			auto processing_queue = new blocking_queue<work_item*>();
			std::thread handler_thread(&socket_server::handle_requests, this, client_socket, processing_queue);
			handler_thread.detach();
			std::thread process_thread(&socket_server::process_requests, this, client_socket, processing_queue);
			process_thread.detach();
		}
	}

	void start(int port)
	{
		boyer_moore2 = new boyer_moore(delimiter);
		struct sockaddr_in server_address;

		WSADATA wsaData;
		WSAStartup(MAKEWORD(2, 2), &wsaData);

		int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		memset((char *)&server_address, 0, sizeof(server_address));
		server_address.sin_family = AF_INET;
		server_address.sin_addr.s_addr = INADDR_ANY;
		server_address.sin_port = htons(port);

		bind(listener, (struct sockaddr *) &server_address, sizeof(server_address));
		listen(listener, 5);

		for (int i = 0; i < 1000; i++)
		{
			std::thread listener_thread(&socket_server::accept_requests, this, listener);
			listener_thread.detach();
		}

		Sleep(1000000 * 1000);
		WSACleanup();
	}

	~socket_server(void)
	{
		delete boyer_moore2;
	}

private:
	void print(std::vector<char>* vector)
	{
		std::string str(vector->begin(), vector->end());
		/*for (auto& i : *vector)
		{
			std::cout << i;
		}*/

		std::cout << L"宗";
		std::cout << str;
		std::cout << std::endl;
	}

	std::vector<std::string>* split(std::string text)
	{
		auto result = new std::vector<std::string>();
		int index = 0;
		for (int i = 0; i < text.length(); i++)
		{
			if (text[i] == '\r' && i + i < text.length() && text[i + 1] == '\n')
			{
				if (i > index)
				{
					(*result).push_back(text.substr(index, i - index));
				}

				index = i + 2;
			}
		}

		if (text.length() > index)
		{
			(*result).push_back(text.substr(index, text.length() - index));
		}

		return result;
	}

	std::vector<std::string>* split(std::string text, char splitter)
	{
		auto result = new std::vector<std::string>();
		int index = 0;
		for (int i = 0; i < text.length(); i++)
		{
			if (text[i] == splitter)
			{
				if (i > index)
				{
					(*result).push_back(text.substr(index, i - index));
				}

				index = i + 1;
			}
		}

		if (text.length() > index)
		{
			(*result).push_back(text.substr(index, text.length() - index));
		}

		return result;
	}
};

int main()
{
	socket_server server;
	server.start(8221);
	return 0;
}



