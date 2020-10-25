#ifndef __NTP_HH__
#define __NTP_HH__

#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <dirkutil.h>
#include <DS3232RTC.h>

#define UDS_BUFSZ 80
//NTPStuff
#define UTC_OFFSET 0

class NTP {

public:
	NTP(WiFiUDP * u);
	NTP(WiFiUDP * u, DS3232RTC *rtc);
	bool begin(void);
	void setRTC(DS3232RTC * rtc);
	time_t doNTP(void);
	time_t get(void);
        int txtStatus(char *buf, int l);
  bool haveNTP(void);
  void setServerName(const char * n);
  IPAddress timeServerIP; // time.nist.gov NTP server address
private:
	const char* ntpServerName = NULL; //"uk.pool.ntp.org";
  DS3232RTC *myRTC = NULL;
	static const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
	
 // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
   static  const unsigned long seventyYears = 2208988800UL;
    
    
	byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
	
	// A UDP instance to let us send and receive packets over UDP
	WiFiUDP  * udp;
	
	unsigned int localPort = 2390;      // local port to listen for UDP packets
	
	bool _got_ntp = false;
	
	const unsigned long ntpIntvlMin = 10000;
	unsigned long _ntpIntvl = ntpIntvlMin;
	const unsigned long ntpIntvlMax = 300000;
	
	unsigned long  _lastslewt = 0;
	
	unsigned long _epoch = 0;
        
        int64_t _ofs = 0;
        int64_t _rtt = 0;

	int _slew = -1;
	
	unsigned long _lastNtpT =0, _currNtpT = 0;
        
        unsigned long getulong(int ofs);
	void setulong(int ofs , unsigned long l);

	void sendNTPpacket(IPAddress& address);

	void ntp_packet(int cb);

};

#endif
