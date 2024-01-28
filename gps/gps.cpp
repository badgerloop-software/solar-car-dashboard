#include "gps.h"
#include <string>
#include <stdio.h>
#include <filesystem>

using namespace std;

vector<string> listUSB() {
    string path = "/dev";
    vector<string> usbdevice;
    for (const auto & entry : filesystem::directory_iterator(path))
        if(((string)entry.path()).find("ttyUSB") != string::npos) {
            std::cout << entry.path() << std::endl;
            usbdevice.push_back((string)entry.path());
        }
        
    return usbdevice;
}


GPS::GPS() {
    listUSB();
}

bool GPS::init(string device) {
    cout << "attempting to open "<<device<<"\n";
    int errorOpen = serial.openDevice(device.c_str(), 115200);
    if(serial.isDeviceOpen()) {
        string resp;
        //flush any previous input
        serial.writeString("\r\n");
        //Set AT no echo back
        serial.writeString("ATE0\r\n");
        //send initital AT
        serial.writeString("AT\r\n");
        serial.readString(resp, '*', 10, 1000);
        cout<<"AT rsp:"<<resp<<"\n";
        if(resp.find("OK") == string::npos) {
            // if does not return ok initialize failed
            return false;
        }
        
        // config gps mode
        serial.writeString("AT+QGPSCFG=\"outport\",\"usbnmea\"\r\n");
        serial.readString(resp, '*', 10, 1000);
        cout<<"QGPSCFG rsp:"<<resp<<"\n";
        if(resp.find("OK") == string::npos) {
            // if does not return ok initialize failed
            return false;
        }

        // turn on gps
        serial.writeString("AT+QGPS=1\r\n");
        serial.readString(resp, '*', 20, 1000);
        cout<<"QGPS rsp:"<<resp<<"\n";
        if(resp.find("OK") == string::npos && resp.find("504") == string::npos) {
            // if not ok or 504 (session ongoing) then init failed
            return false;
        }
        serial.closeDevice();
        return true;
    }
    cout << "Error opening ttyUSB3, check usb connection, are you running with sudo?\n";
    return false;
}

// Degree and decimal minute to decimal degree
string ddm2dd(string ddm) {
    double value = stod(ddm);
    double deg = int(value) / 100;
    double min = value - deg * 100;
    //cout << "raw ddm: " << ddm << " deg: " << deg << " min: " << min << '\n';
    double val = deg + min/60;
    return to_string(val);
}

GPSData parseNMEA(vector<string> nmeaGGA) {
    // Format Docs: https://docs.novatel.com/OEM7/Content/Logs/GPGGA.htm
    GPSData gpsd;
    gpsd.lat = stod(nmeaGGA[3] == "N" ? ddm2dd(nmeaGGA[2]) : '-' + ddm2dd(nmeaGGA[2]));
    gpsd.lon = stod(nmeaGGA[5] == "E" ? ddm2dd(nmeaGGA[4]) : '-' + ddm2dd(nmeaGGA[4]));
    gpsd.alt = stod(nmeaGGA[9]);
    return gpsd;
} 

void GPS::start_loop(string nmeaDevice) {
    cout << fixed;
    cout << setprecision(6);
    // listen on /dev/ttyUSB1 for NMEA sentence
    int errorOpen = serial.openDevice(nmeaDevice.c_str(), 115200);
    if(serial.isDeviceOpen()) {
        cout << "device opened\n";
        int retry_cout = 0;
        while(true){
            string resp;
            int readCode = serial.readString(resp, '$', 300, 1000);
            cout << "readcode: " << readCode << '\n';
            if(readCode == 0) {
                retry_cout ++;
            } else {
                retry_cout = 0;
            }

            if(retry_cout > 5) {
                cout << "break break break" << '\n';
                break;
            }
            // read data from string stream
            vector<string> fields;
            stringstream ss(resp);
            while(ss.good()) {
                string field;
                getline(ss, field, ',');
                fields.push_back(field);
            }
            if (fields[0] == "GPGGA") {
                cout << resp;
                if(fields[6] == "0") {
                    cout << "not fixed\n";
                    lat = -1000;
                    lon = -1000;
                    alt = -1000;
                } else {
                    GPSData gpsd = parseNMEA(fields);
                    lat = gpsd.lat;
                    lon = gpsd.lon;
                    alt = gpsd.alt;
                    cout <<"POS: "<< lat << ", " << lon << '\n'; 
                }
            }
        }
    }
    cout << "Error opening /dev/ttyUSB1..";
    return;
}

GPSData GPS::getLoc(){
    return GPSData{lat, lon, alt};
}

string GPS::usbnmeaprobe(vector<string> devices) {
    for(string device : devices) {
        int errorOpen = serial.openDevice(device.c_str(), 115200);
        if(serial.isDeviceOpen()) {
            string resp;
            int readCode = serial.readString(resp, '$', 300, 1000);
            if (readCode != 0) {
                cout << "device found: "<<device << '\n';
                return device;
            }
        }
    }
    return NULL;
}
void GPS::autoInit() {
    while(true) {
        vector<string>deviceList = listUSB();
        if(deviceList.size() == 0) {
            sleep(1);
            continue;
        }
        int deviceIndex = 0;
        // trial and error to find which device to open since device number is not fixed
        while(true) {
            bool initSuccess = init(deviceList[deviceIndex]);
            cout << "init result: " << initSuccess;
            if (initSuccess) {
                string nmeaDevice = usbnmeaprobe(deviceList);
                start_loop(nmeaDevice);
            } else {
                deviceIndex ++;
            }
            // if all device fail, refresh device list and retry
            if(deviceIndex >= deviceList.size()) {
                break;
            }
            sleep(3);
        }
    }
}
