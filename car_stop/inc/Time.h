//Copyright 2025-2026 Treevar
//All Rights Reserved
#ifndef TIME_H
#define TIME_H
//64 bit time
struct Time{
    using Time_t = uint64_t;
    static const Time_t MAX_TIMESTAMP = UINT64_MAX;
    static const Time_t MS_IN_SEC = 1000;
    static const Time_t MS_IN_MIN = 60 * MS_IN_SEC;
    static const Time_t MS_IN_HOUR = 60 * MS_IN_MIN;
    static const Time_t MS_IN_DAY = 24 * MS_IN_HOUR;
    static const Time_t MS_IN_WEEK = 7 * MS_IN_DAY;
    static const Time NULL_TIME;
    Time_t raw; //Raw number of ms
    Time() : raw(0){}
    Time(const Time &o) : raw(o.raw){}
    Time(Time_t ms, Time_t s = 0, Time_t m = 0, Time_t h = 0, Time_t d = 0, Time_t w = 0) : raw(ms + second(s) + minute(m) + hour(h) + day(d) + week(w)){}
    
    //Returns the difference between 2 times
    static Time timeDelta(Time a, Time b){
        if(b < a){ //a bigger
            return a - b;
        }
        return b - a;
    }

    static Time timeSince(Time t){
        return timeDelta(t, now());
    }

    Time_t seconds() const { //Number of seconds in raw
        return raw / MS_IN_SEC;
    }

    Time_t minutes() const { //Number of minutes in raw
        return raw / MS_IN_MIN;
    }

    Time_t hours() const { //Number of hours in raw
        return raw / MS_IN_HOUR;
    }

    Time_t days() const { //Number of days in raw
        return raw / MS_IN_DAY;
    }

    Time_t weeks() const { //Number of weekss in raw
        return raw / MS_IN_WEEK;
    }

    //Converts seconds to ms
    static Time_t second(Time_t s){
        return s * MS_IN_SEC;
    }

    //Converts minutes to ms
    static Time_t minute(Time_t m){
        return m * MS_IN_MIN;
    }

    //Converts hours to ms
    static Time_t hour(Time_t h){
        return h * MS_IN_HOUR;
    }

    //Converts days to ms
    static Time_t day(Time_t d){
        return d * MS_IN_DAY;
    }

    //Converts weeks to ms
    static Time_t week(Time_t w){
        return w * MS_IN_WEEK;
    }
    //Converts to a time object depending on the number of variables you supply
    //It starts at second and goes up one unit every time (minute, hour, etc)
    //convert(1) = 1 second
    //convert(2, 30) = 2 mins 30 secs
    //convert(1, 0, 30) = 1 hour 30 seconds
    static Time convert(Time_t i0, Time_t i1 = MAX_TIMESTAMP, Time_t i2 = MAX_TIMESTAMP, Time_t i3 = MAX_TIMESTAMP, Time_t i4 = MAX_TIMESTAMP){
        if(i4 != MAX_TIMESTAMP){//week, day, hour, minute, second
            return {week(i0) + day(i1) + hour(i2) + minute(i3) + second(i4)};
        }
        if(i3 != MAX_TIMESTAMP){//day, hour, minute, second
            return {day(i0) + hour(i1) + minute(i2) + second(i3)};
        }
        if(i2 != MAX_TIMESTAMP){//hour, minute, second
            return {hour(i0) + minute(i1) + second(i2)};
        }
        if(i1 != MAX_TIMESTAMP){//minute, second
            return {minute(i0) + second(i1)};
        }
        return {second(i0)};
    }

    //Returns current time after updating it
    static Time now(bool update = true){
        if(update) { updateTime(); }
        return _curTime;
    }

    //Updates the stored current time
    //Since millis() is only 32 bit we need an update function
    static void updateTime(){
        Time time32 {millis()};
        if(_curTime.raw < ULONG_MAX && time32 >= _curTime){ //Still within 32 bits and timer hasnt wrapped around
            _curTime = time32;
        }
        else{ //Timer has wrapped around
            Time curTime32 {_curTime & ULONG_MAX}; //Get only the 32 bit part
            //curTime32 should be less than time32, if its not then the 32 bit timer wrapped around
            if(curTime32 > time32){ //Timer wrapped around
              _curTime.raw += static_cast<uint64_t>(1) << 32;
            }
            uint64_t mask = UINT64_MAX ^ UINT32_MAX; //Mask for upper 32 bits
            _curTime.raw &= mask; //Remove lower 32 bits
            _curTime.raw += time32; //Add current lower 32 bits
        }
    }
    //Converts te time to a human readable format (dd:hh:mm:SS.sss)
    static String toString(Time_t t){
        String out{};
        out.reserve(15);
        Time_t ms[4] = {MS_IN_DAY, MS_IN_HOUR, MS_IN_MIN, MS_IN_SEC};
        String cStr {};
        cStr.reserve(3);
        for(uint8_t i = 0; i < 4; ++i){
            Time_t count = t / ms[i];
            if(count || out.length()){
                cStr = String(count);
                if(cStr.length() < 2 && out.length()){ //Add leading 0 if value is less than 10 and its not the first value
                    out += '0';
                }
                out += cStr;
                out += ':';
                t -= count * ms[i];
            }
        }
        if(out.length()){
            out.setCharAt(out.length()-1, '.');
        }
        else{
            out = "0.";
        }
        cStr = String(t);
        if(cStr.length() == 1 && t){
            cStr += "00";
        }
        else if(cStr.length() == 2){
            cStr += '0';
        }
        out += cStr;
        return out;
    }

    //Converts te time to a human readable format (dd:hh:mm:SS.sss)
    String toString() const {
        return toString(raw);
    }

    //Returns whether the given string is a time (either human readable or raw)
    static bool isTime(const String &timeStr){
        int64_t val = 0;
        if(timeStr[0] == '-'){
            return 0;
        }
        if(isInt(timeStr)){ //Raw
            return 1;
        }
        int idx = timeStr.indexOf('.');
        int endIdx = timeStr.length();
        String part {};
        if(idx > -1){ //Has ms
            part = timeStr.substring(idx+1); //Get ms
            if(!isInt(part)){
                return 0;
            }
            else if((val = toInt(part), val < 0) || val > 999){
                return 0;
            }
            endIdx = idx;
        }
        idx = 0;
        uint8_t i = 0;
        //Only 4 parts max without ms
        for(; i < 4 && endIdx > -1; ++i){
            //Last index bcs we dont know how many parts there are
            idx = timeStr.lastIndexOf(':', endIdx-1);
            //Get the next part (number)
            part = timeStr.substring(idx > -1 ? idx+1 : 0, endIdx);
            endIdx -= part.length() + 1;
            if(!isInt(part)){
                return 0;
            }
            else{ 
                val = toInt(part);
                if(val < 0 || val > 99){
                    return 0;
                }
            }
        }
        if(i == 4 && endIdx > -1){ //String longer than max
            return 0;
        }
        return 1;
    }
    //Converts a string to a time
    //Doesnt do any checking so make sure it is a valid string
    static Time toTime(const String &timeStr){
        Time t {};
        if(isInt(timeStr)){ //Raw
            t.raw = toInt(timeStr);
            return t;
        }
        int64_t val = 0;
        int idx = timeStr.indexOf('.');
        int endIdx = timeStr.length();
        String part {};
        if(idx > -1){ //Has ms
            part = timeStr.substring(idx+1);
            val = toInt(part);
            t.raw += val;
            endIdx = idx;
        }
        idx = 0;
        //Only 4 parts max without ms
        for(uint8_t i = 0; i < 4 && endIdx > -1; ++i){
            //Last index bcs we dont know how many parts there are
            idx = timeStr.lastIndexOf(':', endIdx-1);
            part = timeStr.substring(idx > -1 ? idx+1 : 0, endIdx);
            endIdx -= part.length() + 1;
            if(i == 0){ //Seconds
                t.raw += second(toInt(part));
            }
            else if(i == 1){ //Minutes
                t.raw += minute(toInt(part));
            }
            else if(i == 2){ //Hours
                t.raw += hour(toInt(part));
            }
            else if(i == 3){ //Days
                t.raw += day(toInt(part));
            }
        }
        return t;
    }

    Time operator=(const Time &rhs){
        raw = rhs.raw;
        return *this;
    }

    Time operator=(Time::Time_t rhs){
        raw = rhs;
        return *this;
    }

    Time operator+(const Time &rhs) const {
        return {raw + rhs.raw};
    }

    Time operator+(Time_t rhs) const {
        return {raw + rhs};
    }

    Time operator-(const Time &rhs) const {
        return {raw - rhs.raw};
    }

    Time operator-(Time_t rhs) const {
        return {raw - rhs};
    }

    bool operator<(const Time &rhs) const {
        return raw < rhs.raw;
    }

    bool operator>(const Time_t &rhs) const {
        return raw > rhs;
    }

    bool operator<(const Time_t &rhs) const {
        return raw < rhs;
    }

    bool operator>(const Time &rhs) const {
        return raw > rhs.raw;
    }

    bool operator==(const Time &rhs) const {
        return raw == rhs.raw;
    }

    bool operator!=(const Time &rhs) const {
        return raw != rhs.raw;
    }

    bool operator<=(const Time &rhs) const {
        return raw <= rhs.raw;
    }

    bool operator>=(const Time &rhs) const {
        return raw >= rhs.raw;
    }

    operator Time_t() const {
        return raw;
    }
    private:
    static Time _curTime; //The current time in ms
};
Time Time::_curTime {0};
const Time Time::NULL_TIME {0};
#endif //TIME_H