﻿#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <list>
#include <queue>
#include <mutex>
#include <condition_variable>

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <stdio.h>
#endif

class work_item
{
public:
	int socket;
	char* buffer;
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

	int index_of(std::vector<char> text)
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
	static const int size = 1024;
	boyer_moore* boyer_moore2;

	void handle_requests(int client_socket, blocking_queue<work_item*>* processing_queue)
	{
		int  count;
		char buffer[size];
		std::vector<char> byteBag;
		do
		{
			memset(buffer, 0, size);
			count = recv(client_socket, buffer, size, 0);
			if (count > 0)
			{
				std::vector<char>* vector = new std::vector<char>(buffer, buffer + count);
				int i = boyer_moore2->index_of(*vector);
				if (i != -1)
				{
					vector->resize(i);
					//vector->resize(i+1);
					//vector->push_back(NULL);
					work_item* l = new work_item();
					l->socket = client_socket;
					l->buffer = &(vector->data()[0]);
					processing_queue->push(l);
					processing_queue->finalise();
				}
			}
		} while (count > 0);
	}

	void process_requests(int client_socket, blocking_queue<work_item*>* processing_queue)
	{
		work_item* l;
		do
		{
			l = processing_queue->pop();
			if (l != NULL)
			{
				int s = l->socket;
				printf("%s\r\n", l->buffer);
				//int count = send(client_socket, "200", 18, 0);
				delete l;
			}

		} while (l != NULL);

		closesocket(client_socket);
		printf("socket closed\r\n");
	}

	void accept_requests(int server_socket);

	void start(int port);
};

void socket_server::accept_requests(int server_socket)
{
	struct sockaddr_in client_socket_address;
	int length = sizeof(client_socket_address);
	while (true)
	{
		int client_socket = accept(server_socket, (struct sockaddr *)&client_socket_address, &length);
		blocking_queue<work_item*>* processing_queue = new blocking_queue<work_item*>();
		std::thread handler_thread(&socket_server::handle_requests, this, client_socket, processing_queue);
		handler_thread.detach();
		std::thread process_thread(&socket_server::process_requests, this, client_socket, processing_queue);
		process_thread.detach();
	}
}

void socket_server::start(int port)
{
	boyer_moore2 = new boyer_moore(delimiter);
	WSADATA wsaData;
	int listener;
	struct sockaddr_in serv_addr;
	port = 8221;

	WSAStartup(MAKEWORD(2, 2), &wsaData);
	listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	memset((char *)&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(port);

	bind(listener, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
	listen(listener, 5);

	for (int i = 0; i < 1000; i++)
	{
		std::thread listener_thread(&socket_server::accept_requests, this, listener);
		listener_thread.detach();
	}

	Sleep(1000000 * 1000);
	WSACleanup();
}

int main()
{
	socket_server server;
	server.start(8221);
	return 0;
}



