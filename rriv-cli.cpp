/**
 *  @example example_project.cpp
 */

#include <libserial/SerialPort.h>
#include <libserial/SerialStream.h>

#include <iostream>
#include <cstring>
#include "lib/Cmd.h"
#include <unistd.h>
#include <ctime>
#include <sstream>
#include <fstream>
#include <regex>
#include <filesystem>
#include <dirent.h>
#include "include/filesystem.hpp"

using namespace std;
using namespace LibSerial;


LibSerial::SerialPort * serial_port;
bool processingCommandFile = false;
bool processingTestFile = false;
bool performingRelay = true;
std::ifstream commandFile;
std::string testResult;

void help(int arg_cnt, char **args)
{
    cout << "CLI-Client Command List:" << endl;
    cmdList();
    
    serial_port->Write("help\r\n");
    serial_port->DrainWriteBuffer();
}

void setRTC(int arg_cnt, char **args)
{
    performingRelay = true;
    std::time_t time = std::time(nullptr);
    std::cout << asctime(std::localtime(&time)) << time << std::endl;

    std::stringstream timeStringStream;
    timeStringStream << time;

    serial_port->Write("set-rtc " + timeStringStream.str() + "\r\n");
    serial_port->DrainWriteBuffer();
}

void getRTC(int arg_cnt, char **args)
{
    performingRelay = true;
    std::time_t time = std::time(nullptr);
    //std::cout << asctime(std::localtime(&time)) << time << std::endl;

    std::stringstream timeStringStream;
    timeStringStream << time;

    serial_port->Write("get-rtc " + timeStringStream.str() + "\r\n");
    serial_port->DrainWriteBuffer();
}

void runWorkflow(int arg_cnt, char **args)
{
    processingCommandFile = true;
    commandFile = std::ifstream(args[1]);
}

void runTest(int arg_cnt, char **args)
{
    processingCommandFile = true;
    processingTestFile = true;
    commandFile = std::ifstream(args[1]);
}

void loadSlotFromFile(int arg_cnt, char **args)
{
    std::ifstream slotFile(args[1]);
    // read and remove all return characters
    std::string slotSettings;
    std::string line;
    while (std::getline(slotFile, line))
    {
        line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
        line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());
        line.erase(std::remove(line.begin(), line.end(), ' '), line.end());
        slotSettings = slotSettings + line;
    }
    std::string command = "set-slot-config " + slotSettings;
    std::cout << command << std::endl;
    cmdRun(command.c_str());
    slotFile.close();
}

void loadConfigFromFile(int arg_cnt, char **args)
{
    std::ifstream configFile(args[1]);
    // read and remove all return characters
    std::string config;
    std::string line;
    while (std::getline(configFile, line))
    {
        line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
        line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());
        line.erase(std::remove(line.begin(), line.end(), ' '), line.end());
        config = config + line;
    }
    std::string command = "set-config " + config;
    std::cout << command << std::endl;
    cmdRun(command.c_str());
    configFile.close();
}

void relay(int arg_cnt, char **args)
{
    // cout << "relay" << endl;
    performingRelay = true;
    for (int i = 0; i < arg_cnt; i++)
    {
        serial_port->Write(args[i]);
        if (i < arg_cnt - 1)
        {
            serial_port->Write(" ");
        }
    }
    serial_port->Write("\r\n");
    // cout << "sent" << endl;
}

void echo(int arg_cnt, char ** args)
{
    cout << args[1] << endl;
}

vector<string> list_dir(const char *path, const char *match)
{
    struct dirent *entry;
    DIR *dir = opendir(path);

    vector<string> list = vector<string>();
    if (dir == NULL)
    {
        return list;
    }
    while ((entry = readdir(dir)) != NULL)
    {
        if (match == NULL)
        {
            list.push_back(entry->d_name);
        }
        else
        {
            regex str_expr(match);
            if (regex_match(entry->d_name, str_expr))
                list.push_back(entry->d_name);
        }
    }
    closedir(dir);
    return list;
}

vector<string> list_dir(const char *path)
{
    return list_dir(path, NULL);
}

bool deviceWasOpen = false;
string port;
string devicePort;

int ensureDeviceConnected()
{

    bool deviceSelected = false;
    bool waitForDevice = false;

    if (deviceWasOpen)
    {
        vector<string> list = list_dir("/dev", port.c_str());

        if (list.size() == 0)
        {
            deviceWasOpen = false;
            try 
            {
                serial_port->Close();
                operator delete (serial_port);
                serial_port == NULL;
            } catch (const std::exception &exc)
            {
                std::cerr << exc.what();
            }
            cout << "Device Disconnected" << endl;
            cout << "Please plug in a RRIV device." << endl;
            waitForDevice;
        }
        else
        {
            return 1;
        }
    }

    while (!deviceSelected)
    {

        vector<string> list = list_dir("/dev", "ttyACM.*");
        int i = 1;
        for (const auto &value : list)
        {
            cout << i << ") " << value << "\n";
        }

        if (list.size() == 0)
        {
            if (!waitForDevice)
            {
                cout << "No RRIV devices found." << endl;
                cout << "Please plug in a RRIV device." << endl;
                waitForDevice = true;
            }
            sleep(1);
        }

        if (list.size() == 1)
        {
            port = list.at(0).c_str();
            cout << "Automatically connecting to /dev/" + list.at(0) << endl;
            deviceSelected = true;
        }

        if (list.size() > 1)
        {
            cout << "Please select port to connect to" << endl;
            int portIndex;
            cin >> portIndex;
            port = list.at(portIndex).c_str();
            cout << "Cnnecting to /dev/" + list.at(0) << endl;
            deviceSelected = true;
        }
    }

    std::cin.sync_with_stdio(false);
    using namespace std;

    devicePort = string("/dev/") + port;
    std::cout << "opening " << devicePort << std::endl;

    try
    {
        if (serial_port != NULL)
        {
            // if (serial_port->IsOpen())
            // {
            //     cout << "Close" << endl;
            //     serial_port->Close(); // this will always throw since the /dev/ACM* went away
            // }
            // cout << "Closed" << endl;

            operator delete(serial_port); // operator delete because the destructor is buggy for unplugged /dev/ACM* 
            serial_port = NULL;
        }

        serial_port = new SerialPort();

        std::cout << "opening" << std::endl;
        serial_port->Open(devicePort);

        // serial_port->SetBaudRate(BaudRate::BAUD_115200);
        // serial_port->SetCharacterSize(CharacterSize::CHAR_SIZE_8);
        // serial_port->SetFlowControl(FlowControl::FLOW_CONTROL_NONE);
        // serial_port->SetParity(Parity::PARITY_NONE);
        // serial_port->SetStopBits(StopBits::STOP_BITS_1);

        // serial_port->Write("restart\r\n");
        serial_port->DrainWriteBuffer();
    }
    catch (const std::exception &exc)
    {
        cout << "err" << endl;
        std::cerr << exc.what();

        if (!serial_port->IsOpen())
        {
            cout << "Not  Open" << endl;    
            return -1;
        }
        else
        {
            cout << "Still Open" << endl;
        }
        return -2;
    }

    std::cout << "opened" << std::endl;
    deviceWasOpen = true;
    return 2;
   
}

int main(int argc, char *argv[])
{
    mkdir(".data", 0755);
    mkdir("workflows", 0755);
    mkdir("configs", 0755);

    cmdInit(&std::cin, &std::cout);
    cmdAdd("help", help);
    cmdAdd("set-rtc", setRTC);
    cmdAdd("get-rtc", getRTC);
    cmdAdd("run-workflow", runWorkflow);
    cmdAdd("run-test", runTest);
    cmdAdd("load-config", loadConfigFromFile);
    cmdAdd("load-slot-config", loadSlotFromFile);
    cmdAdd("relay", relay);
    cmdAdd("echo", echo);

    std::string readString;
    

    while (true)
    {
        int result = ensureDeviceConnected();
        // if(result == 2)
        // {   
        //     cmdRun((std::string("run-workflow") + std::string(argv[1])).c_str());
        // }

        usleep(1000);

        while (serial_port->IsDataAvailable())
        {

            try
            {
                serial_port->ReadLine(readString, '\n', 10);
            }
            catch (const std::exception &exc)
            { 
                if(exc.what() == "Read timeout")
                {
                    continue;
                }
                // std::cerr << exc.what();
            }

            // readString = string("CMD >>\r");
            if (readString.size() > 0)
            {
                regex regexExpr(string("CMD >>"));
                if (regex_search(readString, regexExpr))
                {
                    // cout << "skipping" << readString << endl;
                    continue;
                }

                cout << ">";
                cout << readString;
                cout.flush();
            }

            if (processingTestFile)
            {
                if (readString == testResult)
                {
                    std::cout << "Test Success!" << std::endl;
                }
                else
                {
                    std::cout << "Test Failed!" << std::endl;
                }
            }

            usleep(1000);
        }

        if (performingRelay) // wait until we have all the content from the relayed message
        {
            cout << "CMD >> " << endl;
            performingRelay = false;
        }

        if (processingCommandFile)
        {
            std::string line;
            if (std::getline(commandFile, line))
            {
                if (processingTestFile)
                {
                    std::stringstream check1(line);
                    std::string intermediate;
                    std::getline(check1, intermediate, ':');
                    std::string command = intermediate;
                    std::getline(check1, intermediate, ':');
                    testResult = intermediate;
                    cmdRun(command.c_str());
                }
                else
                {
                    // serial_port->Write(line + "\r\n"); // instead pass through Cmd.cpp somehow
                    cmdRun(line.c_str());
                }
            }
            else
            {
                processingCommandFile = false;
                commandFile.close();
            }
        }
        else
        {
            cmdPoll();
        }

        // if(strcmp(buffer, "\u200B\u200B\u200B\u200B\r") == 0)
        // {
        //     std::cout << "will read" << std::endl;
        //     std::cin.getline(buffer, 200);
        //     std::cout << "did read" << std::endl;
        //     // if(strlen(wbuffer) > 0)
        //     // {
        //         std::cout << "done read" << std::endl;
        //         std::cout << buffer << std::endl;
        //     // }
        //     // else
        //     // {
        //         std::cout << "gotnothing" << std::endl;
        //     // }
        // }
        usleep(100000);
    }
}
