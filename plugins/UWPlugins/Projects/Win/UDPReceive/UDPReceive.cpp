// UDPReceive.cpp : Implements a simple receiver that accepts and prints a single broadcast UDP datagram. 
//

#include "PracticalSocket.h"   // For UDPSocket and SocketException
#include <iostream>            // For cout and cerr
#include <cstdlib>             // For atoi()

const int MAXRCVSTRING = 4096; // Longest string to receive


//Function prototypes
void UDPReceiveSimple();
void UDPReceiveByteArray();
void UDPReceiveByteArrayDeliniated();


int main(int argc, char *argv[]) {


	UDPReceiveSimple();
	//UDPReceiveByteArray();
	//UDPReceiveByteArrayDeliniated();

	return 0;
}



/*
Simple example where we wait for a single UDP packet (of chars).
*/
void UDPReceiveSimple()
{
	cout << "UDPReceiveSimple" << endl;

	unsigned short echoServPort = 49003;

	try {
		UDPSocket sock(echoServPort);                

		char recvString[MAXRCVSTRING + 1]; // Buffer for echo string + \0
		string sourceAddress;              // Address of datagram source
		unsigned short sourcePort;         // Port of datagram source

		cout << "Waiting..." << endl;
		int bytesRcvd = sock.recvFrom(recvString, MAXRCVSTRING, sourceAddress, sourcePort);
		recvString[bytesRcvd] = '\0';  // Terminate string

		cout << "Received: " << recvString << endl << endl;
		cout << "bytesRcvd: " << bytesRcvd << endl << endl;
		cout << "sourceAddress:" << sourceAddress << endl << endl;
		cout << "sourcePort: " << sourcePort << endl << endl;

	} catch (SocketException &e) {
		cerr << "PROBLEM!" << endl;
		cerr << e.what() << endl;

	}

	//wait for a dummy entry
	cout << "Press any key then Enter to exit..." << endl;
	int dummy;
	cin >> dummy;
}



/*
Simple example where we wait for a single UDP packet (of bytes).
*/
void UDPReceiveByteArray()
{
	cout << "UDPReceiveByteArray" << endl;

	unsigned short echoServPort = 49003;

	try {
		UDPSocket sock(echoServPort);                

		char recvString[MAXRCVSTRING + 1]; // Buffer for echo string + \0
		string sourceAddress;              // Address of datagram source
		unsigned short sourcePort;         // Port of datagram source

		cout << "Waiting..." << endl;
		int bytesRcvd = sock.recvFrom(recvString, MAXRCVSTRING, sourceAddress, sourcePort);
		recvString[bytesRcvd] = '\0';  // Terminate string

		cout << "Received: " << recvString << endl << endl;
		cout << "bytesRcvd: " << bytesRcvd << endl << endl;
		cout << "sourceAddress:" << sourceAddress << endl << endl;
		cout << "sourcePort: " << sourcePort << endl << endl;

		//Convert the array to the double representation
		double value = atof(recvString);
		cout << "value: " << value << endl << endl;
		
	} catch (SocketException &e) {
		cerr << "PROBLEM!" << endl;
		cerr << e.what() << endl;

	}

	//wait for a dummy entry
	cout << "Press any key then Enter to exit..." << endl;
	int dummy;
	cin >> dummy;
}




/*
Simple example where we wait for a single UDP packet (of bytes).  This is packaged as a space deliniated character array of characters in the order of

	'phiDeg thetaDeg psiDeg latitudeDeg longitudeDeg altitudeMeters'

For example a packet might look like

	'11.123456 -10.123456 90.123456 47.26045 11.34712 914.4'
*/
void UDPReceiveByteArrayDeliniated()
{
	cout << "UDPReceiveByteArrayDeliniated" << endl;

	unsigned short echoServPort = 49003;

	try {
		UDPSocket sock(echoServPort);                

		char recvString[MAXRCVSTRING + 1]; // Buffer for echo string + \0
		string sourceAddress;              // Address of datagram source
		unsigned short sourcePort;         // Port of datagram source

		cout << "Waiting..." << endl;
		int bytesRcvd = sock.recvFrom(recvString, MAXRCVSTRING, sourceAddress, sourcePort);
		recvString[bytesRcvd] = '\0';  // Terminate string

		cout << "Received: " << recvString << endl << endl;
		cout << "bytesRcvd: " << bytesRcvd << endl << endl;
		cout << "sourceAddress:" << sourceAddress << endl << endl;
		cout << "sourcePort: " << sourcePort << endl << endl;

		//Convert the recieved string to values
		double phiDegDouble;
		double thetaDegDouble;
		double psiDegDouble;
		double latitudeDeg;
		double longitudeDeg;
		double altitudeMeters;

		char *pch;			
		pch = strtok(recvString," ");
			
		for(int wordNumber = 0; wordNumber < 6; wordNumber++)
		{
			cout << "wordNumber: " << wordNumber << endl;

			double value = atof(pch);

			if (wordNumber==0) {
				phiDegDouble = value;

			} else if (wordNumber==1) {
				thetaDegDouble = value;

			} else if (wordNumber==2) {
				psiDegDouble = value;

			} else if (wordNumber==3) {
				latitudeDeg = value;

			} else if (wordNumber==4) {
				longitudeDeg = value;

			} else if (wordNumber==5) {
				altitudeMeters = value;

			} else {
				cerr << "ERROR: Unexpected wordNumber" << endl;
			}		

			pch = strtok (NULL, " ");
		}
		
		//convert phi, theta, psi to floats
		float phiDeg	= (float)phiDegDouble;
		float thetaDeg	= (float)thetaDegDouble;
		float psiDeg	= (float)psiDegDouble;

		cout << "DONE" << endl;

		cout << "phiDegDouble: " << phiDegDouble << endl;
		cout << "thetaDegDouble: " << thetaDegDouble << endl;
		cout << "psiDegDouble: " << psiDegDouble << endl;
		cout << endl;
		cout << "phiDeg: " << phiDeg << endl;
		cout << "thetaDeg: " << thetaDeg << endl;
		cout << "psiDeg: " << psiDeg << endl;
		cout << endl;
		cout << "latitudeDeg: " << latitudeDeg << endl;
		cout << "longitudeDeg: " << longitudeDeg << endl;
		cout << "altitudeMeters: " << altitudeMeters << endl;
		
	} catch (SocketException &e) {
		cerr << "PROBLEM!" << endl;
		cerr << e.what() << endl;

	}

	//wait for a dummy entry
	cout << "Press any key then Enter to exit..." << endl;
	int dummy;
	cin >> dummy;
}