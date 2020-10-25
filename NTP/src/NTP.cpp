#include <NTP.h>
#include <TimeLib.h>


  NTP::NTP(WiFiUDP *  u, DS3232RTC *rtc) {
       udp = u;
  	myRTC = rtc;	
 }
 NTP::NTP(WiFiUDP * u) {
 	//Do nothing
       udp = u;
 }

bool NTP::begin(void) {
#ifdef DEBUG 
  DBG_PRINT("Starting UDP\n");
#endif
   udp->begin(localPort);
#ifdef DEBUG 
  DBG_PRINT("Local port: %d\n",udp->localPort());
#endif
  return true;
}

 void NTP::setServerName(const char *n) {
    ntpServerName = n;
 }
 void NTP::setRTC(DS3232RTC * rtc) {
 	myRTC = rtc;	
 }
time_t NTP::doNTP(void) {

  _currNtpT = millis();
  if (
      ((!_got_ntp || timeStatus() == timeNotSet) && (_currNtpT - _lastNtpT) > 10000)
      || (timeStatus() != timeNotSet && (_currNtpT - _lastNtpT) > _ntpIntvl)
     ) {
      WiFi.hostByName(ntpServerName, timeServerIP); 
      DBG_PRINT("Server: %s IP: %x\n", ntpServerName, timeServerIP);
      sendNTPpacket(timeServerIP);
      _lastNtpT = _currNtpT;
      if (timeStatus() != timeNotSet && _got_ntp) {
        _ntpIntvl *= 2;
        if (_ntpIntvl > ntpIntvlMax) _ntpIntvl = ntpIntvlMax;
      }
      _got_ntp = false;
  }
 
  if (int len = udp->parsePacket()) {
    ntp_packet(len);
    _got_ntp = true;
  }
  bool ntpset = false;
  if (_slew > 0 && ((_currNtpT - _lastslewt) > _slew)) {
    if (myRTC) myRTC->set(_epoch);
    setTime (_epoch);
    _slew = 0; 
    ntpset = true;
  }
  time_t t = now();
#ifdef DEBUG
  if (ntpset) {
  	DBG_PRINT("Time Set to: ");
  	digitalClockDisplay(t);
  }
#endif
   return t;
}

time_t NTP::get(void) {
	return doNTP();
}

bool NTP::haveNTP(void) {
	return _got_ntp;
}

int NTP::txtStatus(char * buf, int l) 
{
    char ebuf[40];
    timeStampL(ebuf, 40, (uint64_t)_epoch*1000LL);
    return snprintf(buf, l, "_epoch: %s _slew: %d slewdif: %d tdif: %d _intvl: %d _ofs: %lld _rtt: %lld", ebuf, _slew, _currNtpT - _lastslewt, _currNtpT - _lastNtpT, _ntpIntvl, _ofs, _rtt);
}

// send an NTP request to the time server at the given address
void NTP::sendNTPpacket(IPAddress& address)
{
#ifdef DEBUG
  DBG_PRINT("%s\n","sending NTP packet...");
#endif
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  if (timeStatus() != timeNotSet) { 
    uint64_t lngn = longNow();
    int ms = lngn % 1000;
    setulong(40, (lngn / 1000) + seventyYears);
    //setulong (44, (unsigned long)((double ) ms * 4294967.296D));
    setulong (44, (unsigned long)(( ms * 4294967L) + ((296L *ms) /1000)));
  /* DBG_PRINT("ms = ");
    DBG_PRINT("%d\n",ms);
    unsigned long origt = getulong(24);
    unsigned long origtFrac = getulong(28);
    
    DBG_PRINT("Orig Seconds since Jan 1 1900 = " );
    DBG_PRINT(origt);
    DBG_PRINT (" Fraction = ");
    DBG_PRINT(origtFrac);
    DBG_PRINT (" ms = ");
    DBG_PRINT("%f\n",((double)origtFrac / 4294967296.0D)*1000,2); */
  }
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  //DBG_PRINT("udp %p\n", udp);
  udp->beginPacket(address, 123); //NTP requests are to port 123
  udp->write(packetBuffer, NTP_PACKET_SIZE);
  udp->endPacket();
}

unsigned long NTP::getulong(int ofs) {
    unsigned long highWord = word(packetBuffer[ofs], packetBuffer[ofs+1]);
    unsigned long lowWord = word(packetBuffer[ofs+2], packetBuffer[ofs+3]);
    return highWord << 16 | lowWord;
}

void NTP::setulong(int ofs , unsigned long l) {
 
  packetBuffer[ofs+3] = l & 0xff;
  l >>= 8;
  packetBuffer[ofs+2] = l & 0xff;
  l >>= 8;
  packetBuffer[ofs+1] = l & 0xff;
  l >>= 8;
  packetBuffer[ofs] = l & 0xff;
}


void NTP::ntp_packet(int len) {
   uint64_t recvt_locl = longNow();
   DBG_PRINT("packet received, length=");
   DBG_PRINT("%d\n",len);
    // We've received a packet, read the data from it
    udp->read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:
    unsigned long reft = getulong(16) ;
    reft -= reft > 0 ? seventyYears : 0;
    unsigned long reftFrac = getulong(20);
    unsigned long reftms = (uint32_t) ((((uint64_t)reftFrac) * 100000LL) / (uint64_t)429496729600LL);
    //uint64_t reft_long = (uint64_t)reft*1000 + reftms;

    unsigned long origt = getulong(24);
    origt -= origt > 0 ? seventyYears : 0;
    unsigned long origtFrac = getulong(28);
    unsigned long origtms = (uint32_t) ((((uint64_t)origtFrac) * 100000LL) / (uint64_t)429496729600LL);
    uint64_t origt_long = (uint64_t)origt*1000 + origtms;

    unsigned long recvt = getulong(32);
    recvt -= recvt > 0 ? seventyYears : 0;
    unsigned long recvtFrac = getulong(36);
    unsigned long recvtms = (uint32_t) ((((uint64_t)recvtFrac) * 100000LL) / (uint64_t)429496729600LL);
    uint64_t recvt_long = (uint64_t)recvt*1000 + recvtms;

    _epoch = getulong(40);
    _epoch -= _epoch > 0 ? seventyYears : 0;
    unsigned long frac = getulong(44);
    unsigned long fracms = (uint32_t) ((((uint64_t)frac) * 100000LL) / (uint64_t)429496729600LL);
    uint64_t epoch_long = (uint64_t)_epoch*1000 + fracms;

    _rtt = ((int64_t)recvt_locl - (int64_t)origt_long) - ((int64_t)epoch_long - (int64_t)recvt_long) ;//(T4 - T1) - (T3 - T2)
     _ofs = (((int64_t)recvt_long -(int64_t)origt_long) + ((int64_t)epoch_long - (int64_t)recvt_locl))/2; //((T2 - T1) + (T3 - T4)) / 2.
#ifdef DEBUG
   DBG_PRINT("Ref Seconds = " );
    DBG_PRINT("%d\n", reft);
    //DBG_PRINT ( " Fraction = ");
    //DBG_PRINT(reftFrac);
    DBG_PRINT (" ms = ");
    //DBG_PRINT("%f\n",((double)reftFrac / 4294967296.0D)*1000,2);
    DBG_PRINT("%d\n",reftms );

    DBG_PRINT("Orig Seconds = " );
    DBG_PRINT("%d\n", origt);
    //DBG_PRINT (" Fraction = ");
    //DBG_PRINT("%d\n", origtFrac);
    DBG_PRINT (" ms = ");
    //DBG_PRINT("%f\n",((double)origtFrac / 4294967296.0D)*1000.0D,2);
    DBG_PRINT("%d\n",origtms);

    
    DBG_PRINT("Recv Seconds = " );
    DBG_PRINT("%d\n", recvt);
    //DBG_PRINT (" Fraction = ");
    //DBG_PRINT("%d\n",recvtFrac);
    DBG_PRINT (" ms = ");
    //DBG_PRINT("%f\n",((double)recvtFrac / 4294967296.0D)*1000,2);
    DBG_PRINT("%d\n",recvtms);

    DBG_PRINT("Local recv Seconds = " );
    DBG_PRINT("%d\n", (uint32_t)(recvt_locl /1000));
    DBG_PRINT (" ms = ");
    DBG_PRINT("%d\n",(uint32_t)(recvt_locl % 1000));
    
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    DBG_PRINT("Seconds = " );
    DBG_PRINT("%d\n", _epoch);
    //DBG_PRINT (" Fraction = ");
    //DBG_PRINT("%d\n",frac);
    DBG_PRINT (" ms = ");
    //DBG_PRINT("%f\n",((double)frac / 4294967296.0D)*1000,2);
    DBG_PRINT("%d\n", fracms );

    DBG_PRINT("RTT = ");
    DBG_PRINT("%d\n", _rtt);
    DBG_PRINT("ms OFS = ");
    DBG_PRINT("%d\n", _ofs);
#endif
    int  ofsms = _ofs % 1000;
    if (ofsms > 0) {
      _slew = 1000- ofsms;
      _epoch++;
    } else {
      //slew = 1000 -ofsms;
      _slew = 0 - ofsms;
    }
#ifdef DEBUG
    DBG_PRINT ("slew = ");
    DBG_PRINT("%d\n",_slew);
#endif
    _slew /= 2;
    _lastslewt = millis();
    //for (int del = 0; del < slew / 10; del++) {
    //   delay(10);
    //   yield();
    //}
    //delay(_slew % 10);
    //    DBG_PRINT ("done");

    // now convert NTP time into everyday time:
    //DBG_PRINT("Unix time = ");
   
    // subtract seventy years:
    //unsigned long epoch = secsSince1900 - seventyYears;
    // print Unix time:
    //DBG_PRINT("%d\n",_epoch);
    //int Year = gps.date.year();
    //  byte Month = gps.date.month();
    //  byte Day = gps.date.day();
    //  byte Hour = gps.time.hour();
    //  byte Minute = gps.time.minute();
    //  byte Second = gps.time.second();
        // Set Time from GPS data string
    //    setTime(Hour, Minute, Second, Day, Month, Year);
    //setTime(epoch);
    // Calc current Time Zone time by offset value
    //adjustTime(UTC_OFFSET* SECS_PER_HOUR);  
    // print the hour, minute and second:
    //DBG_PRINT("The UTC time is ");       // UTC is the time at Greenwich Meridian (GMT)
    //DBG_PRINT((epoch  % 86400L) / 3600); // print the hour (86400 equals secs per day)
    //DBG_PRINT(':');
    //if ( ((epoch % 3600) / 60) < 10 ) {
    //  // In the first 10 minutes of each hour, we'll want a leading '0'
    //  DBG_PRINT('0');
    //}
    //DBG_PRINT((epoch  % 3600) / 60); // print the minute (3600 equals secs per minute)
    //DBG_PRINT(':');
    //if ( (_epoch % 60) < 10 ) {
      // In the first 10 seconds of each minute, we'll want a leading '0'
    //  DBG_PRINT('0');
   // }
   // DBG_PRINT("%d\n",_epoch % 60); // print the second
  }
