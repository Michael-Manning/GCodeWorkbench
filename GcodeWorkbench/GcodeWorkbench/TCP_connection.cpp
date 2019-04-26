#include"pch.h"
#include "TCP_connection.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <boost/asio.hpp>
#include <thread>
#include "safe_que.h"


using boost::asio::ip::tcp;
using namespace boost::asio;
using namespace boost::asio::ip;
using namespace std;

bool shouldDiconnect = false;
SafeQueue<Gcommand> cmdQue;
const char* currentAddress;
unsigned short port;
thread com;
volatile bool pause = false;
volatile bool pausePenLifted = false;
int scriptLength;
volatile int commandCounter = 0;
bool connected = false;
vec2 LastReadPosition;
bool readPosition;

void comThread() {

	cout << "attempting to connect" << endl;

	io_service service;
	tcp::socket socket(service);

	try {
		socket.connect(tcp::endpoint(address::from_string(currentAddress), port));
	}
	catch (const std::exception& e) {
		cout << "connection failed" << endl;
		return;
	}
	cout << "connected to: " << currentAddress << endl;
	connected = true;

	while (!shouldDiconnect) {
		Gcommand data;

		if (pause)
		{
			if (!pausePenLifted) {
				data = Gcommand(pen_up_cmd);
				pausePenLifted = true;
			}
			else {
				while (pause)
					std::this_thread::yield();
				data = Gcommand(pen_down_cmd);
			}
		}
		else {
			if (readPosition) {
				data = Gcommand("M114");
			}
			else
				data = cmdQue.dequeue();
		}


		int length = strlen(data.cmd);
		boost::system::error_code error;
		boost::asio::write(socket, boost::asio::buffer(data.cmd, length), error);

		if (!error) {
			cout << "sent: " << data.cmd << endl;
			commandCounter++;
		}
		else {
			cout << "send failed: " << error.message() << endl;
			break;
		}

		// getting response from server
		boost::asio::streambuf receive_buffer;
		boost::asio::read(socket, receive_buffer, boost::asio::transfer_at_least(sizeof(unsigned char) * 2), error);

		if (error && error != boost::asio::error::eof) {
			cout << "receive failed: " << error.message() << endl;
			break;
		}
		else {
			const char* data = boost::asio::buffer_cast<const char*>(receive_buffer.data());
			cout << "Recieved: " << data << endl;
			if (readPosition && strcmp(data, "ok\n")) {

				if (data[0] != 'X')
					continue;

				char* ch =(char*)data;
				char* readFrom = nullptr;

				while (ch && *ch != '\n') {
					//if (*ch < 46 || *ch > 57)
						//*ch = ' ';
					if (*ch == 'M')
						readFrom = ch + 7;
					ch++;
				}
				if (!readFrom)
					continue;

				stringstream ss(readFrom);
				ss >> LastReadPosition.x;
				ss >> LastReadPosition.y;
				cout << "Position updated to:  " << LastReadPosition.x << " " << LastReadPosition.y << endl;
				readPosition = false;
			}
		}
	}
	cout << "Disconnected" << endl;
	socket.close();
	connected = false;
}

void connectTCP(const char* Ip, unsigned short Port)
{
	currentAddress = Ip;
	port = Port;
	com = thread(comThread);
}

void disconnectTCP()
{
	shouldDiconnect = true;
	com.join();
}

void sendCommand(Gcommand command)
{
	if (!connected) 
		cout << "Warning: buffering command as machine is disconnected" << endl;
	
	cmdQue.enqueue(command);
}

void pauseStream()
{
	if (pause)
		return;
	cout << "pauseing stream" << endl;
	pause = true;
	pausePenLifted = false;
}

void resumeStream() {
	pause = false;
	cout << "resuming stream" << endl;
}

float getStreamProgress()
{
	if (!commandCounter)
		return 0;
	return (float)commandCounter / (float)scriptLength;
}

#include <fstream>
#include <string>

void streamFile(const char* filepath)
{
	std::ifstream file(filepath);
	std::string str;
	scriptLength = 0;
	commandCounter = 0;
	while (std::getline(file, str))
	{
		scriptLength++;
		sendCommand(str.c_str());
	}
}

vec2 getPosition() {
	if (!connected)
		return vec2(0);
	readPosition = true;
	sendCommand(Gcommand("G4 P0"));
	while(readPosition)
		std::this_thread::yield();
	return LastReadPosition;
}