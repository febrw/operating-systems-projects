/*
 * CMOS Real-time Clock
 * SKELETON IMPLEMENTATION -- TO BE FILLED IN FOR TASK (1)
 */

/*
 * STUDENT NUMBER: s1735009
 */
#include <infos/drivers/timer/rtc.h>
#include <infos/util/list.h>
#include <infos/util/lock.h>
#include <arch/x86/pio.h>


using namespace infos::drivers;
using namespace infos::drivers::timer;
using namespace infos::util;
using namespace infos::arch::x86;

class CMOSRTC : public RTC {
public:
	static const DeviceClass CMOSRTCDeviceClass;

	const DeviceClass& device_class() const override
	{
		return CMOSRTCDeviceClass;
	}

	/**
	 * Interrogates the RTC to read the current date & time.
	 * @param tp Populates the tp structure with the current data & time, as
	 * given by the CMOS RTC device.
	 */


	void read_timepoint(RTCTimePoint& tp) override
	{
		// FILL IN THIS METHOD - WRITE HELPER METHODS IF NECESSARY
		// Disable interrupts
		UniqueIRQLock lockit;
		bool cycleOccured = false;
		
		// 
		while(true){
			// check update register, activate offset 10 in decimal
			__outb(0x70, 10);
			// read data to v
			uint8_t a = __inb(0x71);
			// set to true if 7th bit is set, false otherwise
			bool inProgress = (a & (1 << 7)) != 0;
			if(inProgress){
				cycleOccured=true;
	
			}
			if(!inProgress && cycleOccured){
				start(tp);
				return;
			}		
		}
	}

	void start(RTCTimePoint& tp){
		// initialise data structures
		// day,month,year
		// hours, minutes,seconds
		List<int> date;
		List<int> time;
		// check if binary or bcd
		__outb(0x70,11);
		uint8_t b = __inb(0x71);
		bool format_binary = (b & (1 << 2)) != 0;
		bool format_24 = (b & (1 << 1)) != 0;
		// load data to buffer
		__outb(0x70,7);
		uint8_t day = __inb(0x71);
		date.append(day);

		__outb(0x70,8);
		uint8_t month = __inb(0x71);
		date.append(month);

		__outb(0x70,9);
		uint8_t year = __inb(0x71);
		date.append(year);

		__outb(0x70,4);
		uint8_t hours = __inb(0x71);
		time.append(hours);

		__outb(0x70,2);
		uint8_t minutes = __inb(0x71);
		time.append(minutes);

		__outb(0x70,0);
		uint8_t seconds = __inb(0x71);
		time.append(seconds);
		             
		if(!format_binary){
			// convert BCD to binary
			for(int i = 0; i < 3; i++){
				// pop item from buffer, convert to binary, append
				uint8_t bcd_date = date.pop();
				uint8_t binary_date = ((bcd_date & 240)>>1) + ((bcd_date & 240)>>3) + (bcd_date & 15);
				date.append(binary_date);

				uint8_t bcd_time = time.pop();
				uint8_t binary_time = ((bcd_time & 240)>>1) + ((bcd_time & 240)>>3) + (bcd_time & 15);
				time.append(binary_time);
			}
		}
		
		if(!format_24){
			// convert to 24
			time.remove(hours);
			// if pm bit is set, convert to 24
			if((hours & (1 << 7)) !=0){
				hours = (hours & 127) + 12;
				if(hours==24){
					hours=0;
				}
				time.push(hours);
			}
		}

		tp.day_of_month = date.at(0);
		tp.month = date.at(1);
		tp.year = date.at(2);
		tp.hours = time.at(0);
		tp.minutes = time.at(1);
		tp.seconds = time.at(2);
		return;
	}
};

const DeviceClass CMOSRTC::CMOSRTCDeviceClass(RTC::RTCDeviceClass, "cmos-rtc");

RegisterDevice(CMOSRTC);
