#include <iostream>
#include <string>
#include <termios.h>
#include <sstream>
#include <locale>
#include <codecvt>
#include <vector>
#include <unordered_map>
#include <fstream>

#include "preCompiled.h"
#include "cO_lin.h"

///////////////////////////////////////////////////////////////////////////////
// SIOOutbound:- Win32 console connection implementation


std::string show_deque(const std::deque<char>& d) {
    std::ostringstream ret;
    ret << "LEN: " << d.size() << std::endl;
    for (char x : d) {
        ret << "\tCHAR: '" << x << "', int: " << std::dec << (int)x << ", hex: " << std::hex << (int)x << ", oct: " << std::oct << int(x) << ";" << std::endl; 
    }
    return ret.str();
}

int cO_lin::busyRead() {
    int ch = EMPTY;
    //input_m.lock();
    if (!in.empty()) {
        //std:: cout << show_deque(in) << std::endl;
        ch = in.front();
        //std::cout << "CHAR (A): '" << (char)ch << "': " << std::hex << int(ch) << std::endl;
        in.pop_front();
    }
    //input_m.unlock();
    return ch;
}

void cO_lin::onKey(bool bDown, int nVirtKey, int lKeyData, int ch)
{
    std::cout << "cO_lin::onKey: bDown: " << bDown << ", nVirtKey: " << nVirtKey << ", lKeyData: " << lKeyData << ", ch: " << ch << std::endl;
    /*
    DWORD nWritten = 0;
    INPUT_RECORD Buffer;
    Buffer.EventType = KEY_EVENT;
    Buffer.Event.KeyEvent.bKeyDown = bDown;
    Buffer.Event.KeyEvent.wRepeatCount = word(lKeyData & 0xFFFF);
    Buffer.Event.KeyEvent.wVirtualKeyCode = word(nVirtKey);
    Buffer.Event.KeyEvent.wVirtualScanCode = word((lKeyData >> 16) & 0xFF);
    Buffer.Event.KeyEvent.uChar.AsciiChar = byte(ch & 0xFF);
    Buffer.Event.KeyEvent.dwControlKeyState = 0;
    if (!::WriteConsoleInput(stdIn, &Buffer, 1, &nWritten) || nWritten < 1)
    {
        dword dw = GetLastError();
        trace("WriteConsoleInput: %d [%08X]\n", dw, dw);
    }
    */
}

/*
bool cO_lin::decode(word vkChar) {
    switch (vkChar) {
        case SDL_SCANCODE_ESCAPE:   strcpy(szVK, "\033\033");   break;
        case SDL_SCANCODE_UP:       strcpy(szVK, "\033A");      break;
        case SDL_SCANCODE_DOWN:     strcpy(szVK, "\033B");      break;
        case SDL_SCANCODE_LEFT:     strcpy(szVK, "\033D");      break;
        case SDL_SCANCODE_RIGHT:    strcpy(szVK, "\033C");      break;
        case SDL_SCANCODE_INSERT:   strcpy(szVK, "\233R");      break;
        case SDL_SCANCODE_DELETE:   strcpy(szVK, "\233S");      break;
        case SDL_SCANCODE_HOME:     strcpy(szVK, "\233G");      break;
        case SDL_SCANCODE_END:      strcpy(szVK, "\233O");      break;
        case SDL_SCANCODE_PAGEUP:   strcpy(szVK, "\033?\156");  break;
        case SDL_SCANCODE_PAGEDOWN: strcpy(szVK, "\033?\115");  break;
        case SDL_SCANCODE_F1:       strcpy(szVK, "\033P");      break;
        case SDL_SCANCODE_F2:       strcpy(szVK, "\033Q");      break;
        case SDL_SCANCODE_F3:       strcpy(szVK, "\033R");      break;
        case SDL_SCANCODE_F4:       strcpy(szVK, "\033S");      break;
        case SDL_SCANCODE_F5:       strcpy(szVK, "\033?\160");  break;
        case SDL_SCANCODE_F6:       strcpy(szVK, "\033?\161");  break;
        case SDL_SCANCODE_F7:       strcpy(szVK, "\033?\162");  break;
        case SDL_SCANCODE_F8:       strcpy(szVK, "\033?\163");  break;
        case SDL_SCANCODE_F9:       strcpy(szVK, "\033?\164");  break;
        case SDL_SCANCODE_F10:      strcpy(szVK, "\033?\165");  break;
        default: return false;
    }
    return true;
}
*/


void cO_lin::decode(char ch) {
    switch (in[0]) {
        case 0x1B: {
            // escape
            if (in.size() == 1) {
                // single escape - double it
                in.push_back('\033');
            } else if (in.size() >= 4) {
                if (in[1] == '\033') {
                    switch (in[2]) {
                        case '[': {
                            switch (in[3]) {
                                case '3': {
                                    if (in.size() == 5 && in[4] == '~') {
                                        // Delete
                                        in.clear();
                                        //strcpy(szVK, "\233S"); 
                                        in.push_back('\233');
                                        in.push_back('S');
                                    }
                                    break;
                                }
                                case '5': {
                                    if (in.size() == 5 && in[4] == '~') {
                                        // PgUp
                                        in.clear();
                                        //strcpy(szVK, "\033?\156");
                                        in.push_back('\033');
                                        in.push_back('S');
                                        in.push_back('\156');
                                    }
                                    break;
                                }
                                case '6': {
                                    if (in.size() == 5 && in[4] == '~') {
                                        // PgDwn
                                        in.clear();
                                        //strcpy(szVK, "\033?\115");
                                        in.push_back('\033');
                                        in.push_back('?');
                                        in.push_back('\115');
                                    }
                                    break;
                                }
                                case 'H': {
                                    if (in.size() == 4) {
                                        // Home
                                        in.clear();
                                        //strcpy(szVK, "\233G");
                                        in.push_back('\233');
                                        in.push_back('G');
                                    }
                                    break;
                                }
                                case 'F': {
                                    if (in.size() == 4) {
                                        // End
                                        in.clear();
                                        //strcpy(szVK, "\233O");
                                        in.push_back('\233');
                                        in.push_back('O');
                                    }
                                    break;
                                }
                                case 'A': {
                                    if (in.size() == 4) {
                                        // key up
                                        in.clear();
                                        in.push_back('\033');
                                        in.push_back('A');
                                    }
                                    break;
                                }
                                case 'B': {
                                    if (in.size() == 4) {
                                        // key down
                                        in.clear();
                                        in.push_back('\033');
                                        in.push_back('B');
                                    }
                                    break;
                                }
                                case 'C': {
                                    if (in.size() == 4) {
                                        // key right
                                        in.clear();
                                        in.push_back('\033');
                                        in.push_back('C');
                                    }
                                    break;
                                }
                                case 'D': {
                                    if (in.size() == 4) {
                                        // key left
                                        in.clear();
                                        in.push_back('\033');
                                        in.push_back('D');
                                    }
                                    break;
                                }
                                default: break;
                            }
                        }
                        case 'O': {
                            switch (in[3]) {
                                case 'P':
                                    if (in.size() == 4) {
                                        // F1
                                        in.clear();
                                        //strcpy(szVK, "\033P");
                                        in.push_back('\033');
                                        in.push_back('P');
                                    }
                                    break;
                                case 'Q':
                                    if (in.size() == 4) {
                                        // F2
                                        in.clear();
                                        //strcpy(szVK, "\033Q");
                                        in.push_back('\033');
                                        in.push_back('Q');
                                    }
                                    break;
                                case 'R':
                                    if (in.size() == 4) {
                                        // F3
                                        in.clear();
                                        //strcpy(szVK, "\033Q");
                                        in.push_back('\033');
                                        in.push_back('R');
                                    }
                                    break;
                                case 'S':
                                    if (in.size() == 4) {
                                        // F4
                                        in.clear();
                                        //strcpy(szVK, "\033S");
                                        in.push_back('\033');
                                        in.push_back('S');
                                    }
                                    break;
                                default: break;
                            }
                            break;
                        }
                        break;
                    }
                }
            }
            break;
        }
        case 0xA: {
            // enter
            if (in.size() == 1) {
                in.push_back('\015');
            }
            break;
        }
        case 0x5B: { // '['
            std::string s;
            for (char x : in) {
                s += x;
            }
            std::cout << "deque: " << s << std::endl;
            // test on arrow key
            if (in.size() == 2) {
                switch (in[1]) {
                    case 0x41: // 'A'
                    case 0x42: // 'B'
                    case 0x43: // 'C'
                    case 0x44: // 'D'
                        in[0] = '\033';
                        std::cout << "ARROW PRESSED: " << in[1] << std::endl; 
                    default: break;
                }
            }
            break;
        }
        default: {
            break;
        }
    }
}

void cO_lin::kbdReader() {
    char ch = '\0';
    while (true) {
        ch = std::cin.get();
        /*
        std::string s;
        for (char x : in) {
            s += x;
        }
        std::cout << "CHAR (B): " << ch << ", deque: " << s << std::endl;
        */
        //input_m.lock();
        in.push_back(ch);
        decode(ch);
        //input_m.unlock();
    }
}

cO_lin::cO_lin() {
    termios settings;
    tcgetattr(fileno(stdin), &settings);
    settings.c_lflag &= (~ICANON & ~ECHO);
    tcsetattr(fileno(stdin), TCSANOW, &settings);
    kbdThread = std::make_unique<std::thread>([this]() { this->kbdReader(); });
}

cO_lin::~cO_lin() { }


int koi8r_ucs[128] = {
  0x2500, 0x2502, 0x250C, 0x2510, 0x2514, 0x2518, 0x251C, 
  0x2524, 0x252C, 0x2534, 0x253C, 0x2580, 0x2584, 0x2588, 0x258C, 0x2590, 
  0x2591, 0x2592, 0x2593, 0x2320, 0x25A0, 0x2219, 0x221A, 0x2248, 0x2264, 
  0x2265, 0x00A0, 0x2321, 0x00B0, 0x00B2, 0x00B7, 0x00F7, 0x2550, 0x2551, 
  0x2552, 0x0451, 0x2553, 0x2554, 0x2555, 0x2556, 0x2557, 0x2558, 0x2559, 
  0x255A, 0x255B, 0x255C, 0x255D, 0x255E, 0x255F, 0x2560, 0x2561, 0x0401, 
  0x2562, 0x2563, 0x2564, 0x2565, 0x2566, 0x2567, 0x2568, 0x2569, 0x256A, 
  0x256B, 0x256C, 0x00A9, 0x044E, 0x0430, 0x0431, 0x0446, 0x0434, 0x0435, 
  0x0444, 0x0433, 0x0445, 0x0438, 0x0439, 0x043A, 0x043B, 0x043C, 0x043D, 
  0x043E, 0x043F, 0x044F, 0x0440, 0x0441, 0x0442, 0x0443, 0x0436, 0x0432, 
  0x044C, 0x044B, 0x0437, 0x0448, 0x044D, 0x0449, 0x0447, 0x044A, 0x042E, 
  0x0410, 0x0411, 0x0426, 0x0414, 0x0415, 0x0424, 0x0413, 0x0425, 0x0418, 
  0x0419, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E, 0x041F, 0x042F, 0x0420, 
  0x0421, 0x0422, 0x0423, 0x0416, 0x0412, 0x042C, 0x042B, 0x0417, 0x0428, 
  0x042D, 0x0429, 0x0427, 0x042A};

int cp866_ucs[128] = {
  0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 
  0x0417, 0x0418, 0x0419, 0x041a, 0x041b, 0x041c, 0x041d, 0x041e, 0x041f, 
  0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427, 0x0428, 
  0x0429, 0x042a, 0x042b, 0x042c, 0x042d, 0x042e, 0x042f, 0x0430, 0x0431, 
  0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437, 0x0438, 0x0439, 0x043a, 
  0x043b, 0x043c, 0x043d, 0x043e, 0x043f, 0x2591, 0x2592, 0x2593, 0x2502, 
  0x2524, 0x2561, 0x2562, 0x2556, 0x2555, 0x2563, 0x2551, 0x2557, 0x255d, 
  0x255c, 0x255b, 0x2510, 0x2514, 0x2534, 0x252c, 0x251c, 0x2500, 0x253c, 
  0x255e, 0x255f, 0x255a, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256c, 
  0x2567, 0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256b, 
  0x256a, 0x2518, 0x250c, 0x2588, 0x2584, 0x258c, 0x2590, 0x2580, 0x0440, 
  0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447, 0x0448, 0x0449, 
  0x044a, 0x044b, 0x044c, 0x044d, 0x044e, 0x044f, 0x0401, 0x0451, 0x0404, 
  0x0454, 0x0407, 0x0457, 0x040e, 0x045e, 0x00b0, 0x2219, 0x00b7, 0x221a, 
  0x2116, 0x00a4, 0x25a0, 0x00a0};

int iso8859_5_ucs[128] = {
  0x0080, 0x0081, 0x0082, 0x0083, 0x0084, 0x0085, 0x0086, 
  0x0087, 0x0088, 0x0089, 0x008A, 0x008B, 0x008C, 0x008D, 0x008E, 0x008F, 
  0x0090, 0x0091, 0x0092, 0x0093, 0x0094, 0x0095, 0x0096, 0x0097, 0x0098, 
  0x0099, 0x009A, 0x009B, 0x009C, 0x009D, 0x009E, 0x009F, 0x00A0, 0x0401, 
  0x0402, 0x0403, 0x0404, 0x0405, 0x0406, 0x0407, 0x0408, 0x0409, 0x040A, 
  0x040B, 0x040C, 0x00AD, 0x040E, 0x040F, 0x0410, 0x0411, 0x0412, 0x0413, 
  0x0414, 0x0415, 0x0416, 0x0417, 0x0418, 0x0419, 0x041A, 0x041B, 0x041C, 
  0x041D, 0x041E, 0x041F, 0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 
  0x0426, 0x0427, 0x0428, 0x0429, 0x042A, 0x042B, 0x042C, 0x042D, 0x042E, 
  0x042F, 0x0430, 0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437, 
  0x0438, 0x0439, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E, 0x043F, 0x0440, 
  0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447, 0x0448, 0x0449, 
  0x044A, 0x044B, 0x044C, 0x044D, 0x044E, 0x044F, 0x2116, 0x0451, 0x0452, 
  0x0453, 0x0454, 0x0455, 0x0456, 0x0457, 0x0458, 0x0459, 0x045A, 0x045B, 
  0x045C, 0x00A7, 0x045E, 0x045F};

int cp1251_ucs[128] = {
  0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 
  0x2021, 0x20AC, 0x2030, 0x0409, 0x2039, 0x040A, 0x040C, 0x040B, 0x040F, 
  0x0452, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0, 
  0x2122, 0x0459, 0x203A, 0x045A, 0x045C, 0x045B, 0x045F, 0x00A0, 0x040E, 
  0x045E, 0x0408, 0x00A4, 0x0490, 0x00A6, 0x00A7, 0x0401, 0x00A9, 0x0404, 
  0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x0407, 0x00B0, 0x00B1, 0x0406, 0x0456, 
  0x0491, 0x00B5, 0x00B6, 0x00B7, 0x0451, 0x2116, 0x0454, 0x00BB, 0x0458, 
  0x0405, 0x0455, 0x0457, 0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 
  0x0416, 0x0417, 0x0418, 0x0419, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E, 
  0x041F, 0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427, 
  0x0428, 0x0429, 0x042A, 0x042B, 0x042C, 0x042D, 0x042E, 0x042F, 0x0430, 
  0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437, 0x0438, 0x0439, 
  0x043A, 0x043B, 0x043C, 0x043D, 0x043E, 0x043F, 0x0440, 0x0441, 0x0442, 
  0x0443, 0x0444, 0x0445, 0x0446, 0x0447, 0x0448, 0x0449, 0x044A, 0x044B, 
  0x044C, 0x044D, 0x044E, 0x044F};

int maccyr_ucs[128] = {
  0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 0x0417, 0x0418, 
  0x0419, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E, 0x041F, 0x0420, 0x0421, 
  0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427, 0x0428, 0x0429, 0x042A, 
  0x042B, 0x042C, 0x042D, 0x042E, 0x042F, 0x2020, 0x00B0, 0x0490, 0x00A3, 
  0x00A7, 0x2022, 0x00B6, 0x0406, 0x00AE, 0x00A9, 0x2122, 0x0402, 0x0452, 
  0x2260, 0x0403, 0x0453, 0x221E, 0x00B1, 0x2264, 0x2265, 0x0456, 0x00B5, 
  0x0491, 0x0408, 0x0404, 0x0454, 0x0407, 0x0457, 0x0409, 0x0459, 0x040A, 
  0x045A, 0x0458, 0x0405, 0x00AC, 0x221A, 0x0192, 0x2248, 0x2206, 0x00AB, 
  0x00BB, 0x2026, 0x00A0, 0x040B, 0x045B, 0x040C, 0x045C, 0x0455, 0x2013, 
  0x2014, 0x201C, 0x201D, 0x2018, 0x2019, 0x00F7, 0x201E, 0x040E, 0x045E, 
  0x040F, 0x045F, 0x2116, 0x0401, 0x0451, 0x044F, 0x0430, 0x0431, 0x0432, 
  0x0433, 0x0434, 0x0435, 0x0436, 0x0437, 0x0438, 0x0439, 0x043A, 0x043B, 
  0x043C, 0x043D, 0x043E, 0x043F, 0x0440, 0x0441, 0x0442, 0x0443, 0x0444, 
  0x0445, 0x0446, 0x0447, 0x0448, 0x0449, 0x044A, 0x044B, 0x044C, 0x044D, 
  0x044E, 0x20AC};

int alt_m_ucs[128] = {
  0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 
  0x0417, 0x0418, 0x0419, 0x041a, 0x041b, 0x041c, 0x041d, 0x041e, 0x041f, 
  0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427, 0x0428, 
  0x0429, 0x042a, 0x042b, 0x042c, 0x042d, 0x042e, 0x042f, 0x0430, 0x0431, 
  0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437, 0x0438, 0x0439, 0x043a, 
  0x043b, 0x043c, 0x043d, 0x043e, 0x043f, 0x2591, 0x2592, 0x2593, 0x2502, 
  0x2524, 0x2561, 0x2562, 0x2556, 0x2555, 0x2563, 0x2551, 0x2557, 0x255d, 
  0x255c, 0x255b, 0x2510, 0x2514, 0x2534, 0x252c, 0x251c, 0x2500, 0x253c, 
  0x255e, 0x255f, 0x255a, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256c, 
  0x2567, 0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256b, 
  0x256a, 0x2518, 0x250c, 0x2588, 0x2584, 0x258c, 0x2590, 0x2580, 0x0440, 
  0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447, 0x0448, 0x0449, 
  0x044a, 0x044b, 0x044c, 0x044d, 0x044e, 0x044f, 0x0401, 0x0451, 0x2265, 
  0x2264, 0x2320, 0x2321, 0x00f7, 0x2248, 0x00b0, 0x2219, 0x00b7, 0x221a, 
  0x207f, 0x00b2, 0x25a0, 0x00a0};

int alt_ucs[128] = {
  0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 
  0x0417, 0x0418, 0x0419, 0x041a, 0x041b, 0x041c, 0x041d, 0x041e, 0x041f, 
  0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427, 0x0428, 
  0x0429, 0x042a, 0x042b, 0x042c, 0x042d, 0x042e, 0x042f, 0x0430, 0x0431, 
  0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437, 0x0438, 0x0439, 0x043a, 
  0x043b, 0x043c, 0x043d, 0x043e, 0x043f, 0x2591, 0x2592, 0x2593, 0x2502, 
  0x2524, 0x2561, 0x2562, 0x2556, 0x2555, 0x2563, 0x2551, 0x2557, 0x255d, 
  0x255c, 0x255b, 0x2510, 0x2514, 0x2534, 0x252c, 0x251c, 0x2500, 0x253c, 
  0x255e, 0x255f, 0x255a, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256c, 
  0x2567, 0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256b, 
  0x256a, 0x2518, 0x250c, 0x2588, 0x2584, 0x258c, 0x2590, 0x2580, 0x0440, 
  0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447, 0x0448, 0x0449, 
  0x044a, 0x044b, 0x044c, 0x044d, 0x044e, 0x044f, 0x0401, 0x0451, 0xe000, 
  0xe001, 0xe002, 0xe003, 0x2192, 0x2190, 0x2193, 0x2191, 0x00f7, 0x00b1, 
  0x2116, 0x00a4, 0x25a0, 0x00a0};

int main_ucs[128] = {
  0x2567, 0x2568, 0x2564, 0x2561, 0x2562, 0x2556, 0x2555, 0x2565, 0x2559, 
  0x2558, 0x2552, 0x255c, 0x255b, 0x255e, 0x255f, 0x2553, 0x2554, 0x2557, 
  0x255d, 0x255a, 0x2550, 0x2551, 0x2566, 0x2563, 0x2569, 0x2560, 0x256c, 
  0x2591, 0x2592, 0x2593, 0x256b, 0x256a, 0x250c, 0x2510, 0x2518, 0x2514, 
  0x2500, 0x2502, 0x252c, 0x2524, 0x2534, 0x251c, 0x253c, 0x2588, 0x2584, 
  0x258c, 0x2590, 0x2580, 0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 
  0x0416, 0x0417, 0x0418, 0x0419, 0x041a, 0x041b, 0x041c, 0x041d, 0x041e, 
  0x041f, 0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427, 
  0x0428, 0x0429, 0x042a, 0x042b, 0x042c, 0x042d, 0x042e, 0x042f, 0x0430, 
  0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437, 0x0438, 0x0439, 
  0x043a, 0x043b, 0x043c, 0x043d, 0x043e, 0x043f, 0x0440, 0x0441, 0x0442, 
  0x0443, 0x0444, 0x0445, 0x0446, 0x0447, 0x0448, 0x0449, 0x044a, 0x044b, 
  0x044c, 0x044d, 0x044e, 0x044f, 0x0401, 0x0451, 0xe000, 0xe001, 0xe002, 
  0xe003, 0x2192, 0x2190, 0x2193, 0x2191, 0x00f7, 0x00b1, 0x2116, 0x00a4, 
  0x25a0, 0x00a0};

int bulg_ucs[128] = {
  0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 0x0417, 0x0418, 
  0x0419, 0x041a, 0x041b, 0x041c, 0x041d, 0x041e, 0x041f, 0x0420, 0x0421, 
  0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427, 0x0428, 0x0429, 0x042a, 
  0x042b, 0x042c, 0x042d, 0x042e, 0x042f, 0x0430, 0x0431, 0x0432, 0x0433, 
  0x0434, 0x0435, 0x0436, 0x0437, 0x0438, 0x0439, 0x043a, 0x043b, 0x043c, 
  0x043d, 0x043e, 0x043f, 0x0440, 0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 
  0x0446, 0x0447, 0x0448, 0x0449, 0x044a, 0x044b, 0x044c, 0x044d, 0x044e, 
  0x044f, 0x2514, 0x2534, 0x252c, 0x251c, 0x2500, 0x253c, 0x2563, 0x2551, 
  0x255a, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256c, 0x2510, 0x2591, 
  0x2592, 0x2593, 0x2502, 0x2524, 0x2116, 0x00a4, 0x2557, 0x255d, 0x2518, 
  0x250c, 0x2588, 0x2584, 0x258c, 0x2590, 0x2580, 0x03b1, 0x00df, 0x0393, 
  0x03c0, 0x03a3, 0x03c3, 0x00b5, 0x03c4, 0x03a6, 0x0398, 0x03a9, 0x03b4, 
  0x221e, 0x03c6, 0x03b5, 0x2229, 0x2261, 0x00b1, 0x2265, 0x2264, 0x2320, 
  0x2321, 0x00f7, 0x2248, 0x00b0, 0x2219, 0x00b7, 0x221a, 0x207f, 0x00b2, 
  0x25a0, 0x00a0};

int koi8_ucs[128] = {
  0x2514, 0x2534, 0x252c, 0x251c, 0x2500, 0x253c, 0x255e, 0x255f, 0x255a, 
  0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256c, 0x2567, 0x2568, 0x2564, 
  0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256b, 0x256a, 0x2518, 0x250c, 
  0x2588, 0x2584, 0x258c, 0x2590, 0x2580, 0x00e1, 0x00ed, 0x00f3, 0x00fa, 
  0x00f1, 0x00d1, 0x00aa, 0x00ba, 0x00bf, 0x2310, 0x00ac, 0x00bd, 0x00bc, 
  0x00a1, 0x00ab, 0x00bb, 0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 
  0x2562, 0x2556, 0x2555, 0x2563, 0x2551, 0x2557, 0x255d, 0x255c, 0x255b, 
  0x2510, 0x044e, 0x0430, 0x0431, 0x0446, 0x0434, 0x0435, 0x0444, 0x0433, 
  0x0445, 0x0438, 0x0439, 0x043a, 0x043b, 0x043c, 0x043d, 0x043e, 0x043f, 
  0x044f, 0x0440, 0x0441, 0x0442, 0x0443, 0x0436, 0x0432, 0x044c, 0x044b, 
  0x0437, 0x0448, 0x044d, 0x0449, 0x0447, 0x044a, 0x042e, 0x0410, 0x0411, 
  0x0426, 0x0414, 0x0415, 0x0424, 0x0413, 0x0425, 0x0418, 0x0419, 0x041a, 
  0x041b, 0x041c, 0x041d, 0x041e, 0x041f, 0x042f, 0x0420, 0x0421, 0x0422, 
  0x0423, 0x0416, 0x0412, 0x042c, 0x042b, 0x0417, 0x0428, 0x042d, 0x0429, 
  0x0427, 0x00a0};

using Table = std::unordered_map<int, int>;

Table inverse_table(int tab[], int size = 128) {
    Table ret;
    for (int i = 0; i < size; ++i) {
        ret[tab[i]] = i;
    }
    return ret;
}
Table direct_table(int tab[], int size = 128) {
    Table ret;
    for (int i = 0; i < size; ++i) {
        ret[i] = tab[i];
    }
    return ret;
}

int apply(const Table& table, int ch) {
    if (table.find(ch) != table.end()) {
        return table.at(ch);
    } else {
        return ch;
    }
}

int apply_many(const std::vector<Table>& tables, int ch) {
    for (const auto& table: tables) {
        ch = apply(table, ch);
    }
    return ch;
}

/*
int koi8r_ucs[128] = {
  0x2500, 0x2502, 0x250C, 0x2510, 0x2514, 0x2518, 0x251C, 
  0x2524, 0x252C, 0x2534, 0x253C, 0x2580, 0x2584, 0x2588, 0x258C, 0x2590, 
  0x2591, 0x2592, 0x2593, 0x2320, 0x25A0, 0x2219, 0x221A, 0x2248, 0x2264, 
  0x2265, 0x00A0, 0x2321, 0x00B0, 0x00B2, 0x00B7, 0x00F7, 0x2550, 0x2551, 
  0x2552, 0x0451, 0x2553, 0x2554, 0x2555, 0x2556, 0x2557, 0x2558, 0x2559, 
  0x255A, 0x255B, 0x255C, 0x255D, 0x255E, 0x255F, 0x2560, 0x2561, 0x0401, 
  0x2562, 0x2563, 0x2564, 0x2565, 0x2566, 0x2567, 0x2568, 0x2569, 0x256A, 
  0x256B, 0x256C, 0x00A9, 0x044E, 0x0430, 0x0431, 0x0446, 0x0434, 0x0435, 
  0x0444, 0x0433, 0x0445, 0x0438, 0x0439, 0x043A, 0x043B, 0x043C, 0x043D, 
  0x043E, 0x043F, 0x044F, 0x0440, 0x0441, 0x0442, 0x0443, 0x0436, 0x0432, 
  0x044C, 0x044B, 0x0437, 0x0448, 0x044D, 0x0449, 0x0447, 0x044A, 0x042E, 
  0x0410, 0x0411, 0x0426, 0x0414, 0x0415, 0x0424, 0x0413, 0x0425, 0x0418, 
  0x0419, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E, 0x041F, 0x042F, 0x0420, 
  0x0421, 0x0422, 0x0423, 0x0416, 0x0412, 0x042C, 0x042B, 0x0417, 0x0428, 
  0x042D, 0x0429, 0x0427, 0x042A
};
*/

inline
std::string cpp20_codepoint_to_utf8(char32_t cp) // C++20 Sandard
{
    using codecvt_32_8_type = std::codecvt<char32_t, std::uint8_t, std::mbstate_t>;

    struct codecvt_utf8 : public codecvt_32_8_type {
        codecvt_utf8(std::size_t refs = 0): codecvt_32_8_type(refs) {}
    };

    std::uint8_t utf8[4];
    std::uint8_t* end_of_utf8;

    char32_t const* from = &cp;

    std::mbstate_t mbs;
    codecvt_utf8 ccv;

    if(ccv.out(mbs, from, from + 1, from, utf8, utf8 + 4, end_of_utf8)) {
        throw std::runtime_error("bad conversion");
    }

    return {reinterpret_cast<char*>(utf8), reinterpret_cast<char*>(end_of_utf8)};
}

void decode_866(char ch, std::string& s) {
    //s += ch;
    //return;
    if (ch >= 0) {
        s += ch;
    } else {
        /*
        AVAILABLE:
            koi8r_ucs
            cp866_ucs
            iso8859_5_ucs
            cp1251_ucs
            maccyr_ucs
            alt_m_ucs
            alt_ucs
            main_ucs
            bulg_ucs
            koi8_ucs
        */
        //ch = -ch;
        std::wostringstream os;
        std::wstring string_to_convert;
        //setup converter
        using convert_type = std::codecvt_utf8<char16_t>;
        std::wstring_convert<convert_type, char16_t> converter;

        static Table koi8r = direct_table(koi8r_ucs);
        static Table cp866 = direct_table(cp866_ucs);
        static Table cp1251 = direct_table(cp1251_ucs);

        static Table un_koi8r = inverse_table(koi8r_ucs);
        static Table un_cp866 = inverse_table(cp866_ucs);
        static Table un_cp1251 = inverse_table(cp1251_ucs);

        int code = apply_many({koi8r}, 128 + ch);

        //int code = apply_many({koi8r}, -ch);
        //int code = apply_many({cp866}, -ch);
        //int code = apply_many({cp1251}, -ch);

        //int code = apply_many({un_cp866, koi8r}, -ch);
        //int code = apply_many({un_cp1251, koi8r}, -ch);
        //int code = apply_many({un_koi8r, cp866}, -ch);
        //int code = apply_many({un_cp1251, cp866}, -ch);
        //int code = apply_many({un_cp866, cp1251}, -ch);
        //int code = apply_many({un_koi8r, cp1251}, -ch);

        //os << static_cast<char16_t>(code);

        //s += converter.to_bytes(static_cast<char16_t>(code));
        //s += -ch;
        //s += cpp20_codepoint_to_utf8(code);
        //return;

        uint8_t ch1 = static_cast<uint8_t>(ch);
        switch (ch1) {
            case 0x80: s += "\u2568"; break; // "╨"
            case 0x81: s += "\u2564"; break; // "╤"
            case 0x82: s += "\u2565"; break; // "╥"
            case 0x83: s += "\u2559"; break; // "╙"
            case 0x84: s += "\u2558"; break; // "╘"
            case 0x85: s += "\u2552"; break; // "╒"
            case 0x86: s += "\u2553"; break; // "╓"
            case 0x87: s += "\u256B"; break; // "╫"
            case 0x88: s += "\u256A"; break; // "╪"
            case 0x89: s += "\u2518"; break; // "┘"
            case 0x8A: s += "\u250C"; break; // "┌"
            case 0x8B: s += "\u2588"; break; // "█"
            case 0x8C: s += "\u2584"; break; // "▄"
            case 0x8D: s += "\u258C"; break; // "▌"
            case 0x8E: s += "\u2590"; break; // "▐"
            case 0x8F: s += "\u2580"; break; // "▀"

            case 0x90: s += "\u2591"; break; // "░"
            case 0x91: s += "\u2592"; break; // "▒"
            case 0x92: s += "\u2593"; break; // "▓"
            case 0x93: s += "\u2502"; break; // "│"
            case 0x94: s += "\u2524"; break; // "┤"
            case 0x95: s += "\u2561"; break; // "╡"
            case 0x96: s += "\u2562"; break; // "╢"
            case 0x97: s += "\u2556"; break; // "╖"
            case 0x98: s += "\u2555"; break; // "╕"
            case 0x99: s += "\u2563"; break; // "╣"
            case 0x9A: s += "\u2551"; break; // "║"
            case 0x9B: s += "\u2557"; break; // "╗"
            case 0x9C: s += "\u255D"; break; // "╝"
            case 0x9D: s += "\u255C"; break; // "╜"
            case 0x9E: s += "\u255B"; break; // "╛"
            case 0x9F: s += "\u2510"; break; // "┐"

            case 0xA0: s += "\u2514"; break; // "└"
            case 0xA1: s += "\u2534"; break; // "┴"
            case 0xA2: s += "\u252C"; break; // "┬"
            case 0xA3: s += "\u251C"; break; // "├"
            case 0xA4: s += "\u2500"; break; // "─"
            case 0xA5: s += "\u253C"; break; // "┼"
            case 0xA6: s += "\u255E"; break; // "╞"
            case 0xA7: s += "\u255F"; break; // "╟"
            case 0xA8: s += "\u255A"; break; // "╚"
            case 0xA9: s += "\u2554"; break; // "╔"
            case 0xAA: s += "\u2569"; break; // "╩"
            case 0xAB: s += "\u2566"; break; // "╦"
            case 0xAC: s += "\u2560"; break; // "╠"
            case 0xAD: s += "\u2550"; break; // "═"
            case 0xAE: s += "\u256C"; break; // "╬"
            case 0xAF: s += "\u2567"; break; // "╧"

            case 0xB0: s += "\u2591"; break; // "░"
            case 0xB1: s += "\u2592"; break; // "▒"
            case 0xB2: s += "\u2593"; break; // "▓"
            case 0xB3: s += "\u2502"; break; // "│"
            case 0xB4: s += "\u2524"; break; // "┤"
            case 0xB5: s += "\u2561"; break; // "╡"
            case 0xB6: s += "\u2562"; break;
            case 0xB7: s += "\u2556"; break;
            case 0xB8: s += "\u2555"; break;
            case 0xB9: s += "\u2563"; break;
            case 0xBA: s += "\u2551"; break;
            case 0xBB: s += "\u2557"; break;
            case 0xBC: s += "\u255D"; break;
            case 0xBD: s += "\u255C"; break;
            case 0xBE: s += "\u255B"; break;
            case 0xBF: s += "\u2510"; break;

            case 0xC0: s += "\u044E"; break; // ю
            case 0xC1: s += "\u0430"; break; // а
            case 0xC2: s += "\u0431"; break; // б
            case 0xC3: s += "\u0446"; break; // ц
            case 0xC4: s += "\u0434"; break; // д
            case 0xC5: s += "\u0435"; break; // е
            case 0xC6: s += "\u0444"; break; // ф 
            case 0xC7: s += "\u0433"; break; // г 
            case 0xC8: s += "\u0445"; break; // х 
            case 0xC9: s += "\u0438"; break; // и 
            case 0xCA: s += "\u0439"; break; // й 
            case 0xCB: s += "\u043A"; break; // к
            case 0xCC: s += "\u043B"; break; // л
            case 0xCD: s += "\u043C"; break; // м
            case 0xCE: s += "\u043D"; break; // н
            case 0xCF: s += "\u043E"; break; // о

            case 0xD0: s += "\u043F"; break; // п
            case 0xD1: s += "\u044F"; break; // я
            case 0xD2: s += "\u0440"; break; // р
            case 0xD3: s += "\u0441"; break; // с
            case 0xD4: s += "\u0442"; break; // т
            case 0xD5: s += "\u0443"; break; // у
            case 0xD6: s += "\u0436"; break; // ж
            case 0xD7: s += "\u0432"; break; // в
            case 0xD8: s += "\u044C"; break; // ь
            case 0xD9: s += "\u044B"; break; // ы
            case 0xDA: s += "\u0437"; break; // з
            case 0xDB: s += "\u0448"; break; // ш
            case 0xDC: s += "\u044D"; break; // э
            case 0xDD: s += "\u0449"; break; // щ
            case 0xDE: s += "\u0447"; break; // ч
            case 0xDF: s += "\u044A"; break; // ъ

            case 0xE0: s += "\u042E"; break; // Ю
            case 0xE1: s += "\u0410"; break; // А
            case 0xE2: s += "\u0411"; break; // Б
            case 0xE3: s += "\u0426"; break; // Ц
            case 0xE4: s += "\u0414"; break; // Д
            case 0xE5: s += "\u0415"; break; // Е
            case 0xE6: s += "\u0424"; break; // Ф
            case 0xE7: s += "\u0413"; break; // Г
            case 0xE8: s += "\u0425"; break; // Х
            case 0xE9: s += "\u0418"; break; // И
            case 0xEA: s += "\u0419"; break; // Й
            case 0xEB: s += "\u041A"; break; // К
            case 0xEC: s += "\u041B"; break; // Л
            case 0xED: s += "\u041C"; break; // М
            case 0xEE: s += "\u041D"; break; // Н
            case 0xEF: s += "\u041E"; break; // О

            case 0xF0: s += "\u041F"; break; // П
            case 0xF1: s += "\u042F"; break; // Я
            case 0xF2: s += "\u0420"; break; // Р
            case 0xF3: s += "\u0421"; break; // С
            case 0xF4: s += "\u0422"; break; // Т
            case 0xF5: s += "\u0423"; break; // У
            case 0xF6: s += "\u0416"; break; // Ж
            case 0xF7: s += "\u0412"; break; // В
            case 0xF8: s += "\u042C"; break; // Ь
            case 0xF9: s += "\u042B"; break; // Ы
            case 0xFA: s += "\u0417"; break; // З
            case 0xFB: s += "\u0428"; break; // Ш
            case 0xFC: s += "\u042D"; break; // Э
            case 0xFD: s += "\u0429"; break; // Щ
            case 0xFE: s += "\u0427"; break; // Ч
            case 0xFF: s += "\u042A"; break; // Ъ
            default: s += ch;
        }
    }
}

std::string show_str(const char* ptr, int i) {
    std::ostringstream ret;
    for (int j = i;  j < i + 3; ++j) {
        char ch = ptr[j];
        ret << "\tCHAR: '" << ch << "', int: " << std::dec << (int)ch << ", hex: " << std::hex << (int)ch << ", oct: " << std::oct << int(ch) << ";" << std::endl; 
    }
    return ret.str();
}

void cO_lin::write(char *ptr, int bytes) {
    //static std::string s;
    //s.clear();
    for (int i = 0; i < bytes;  ++ i) {
        //decode_866(*ptr++, s);
        if (ptr[i] == '\033') {
            std::cout << "DETECED:\n"  << show_str(ptr, i) << std::endl;
            if (i + 2 < bytes && ptr[i + 1] == '[') {
                if (i + 3 < bytes && ptr[i + 2] == '7' && ptr[i + 3] == 'm') {
                    std::cout << "\033[7;42m";
                } else {
                    writeChar(ptr[i]);
                }
            } else if (ptr[i + 2] == 'm') {
                std::cout << "\033[0m";
                writeChar(ptr[i]);
            } else {
                writeChar(ptr[i]);
            }
        } else {
            writeChar(ptr[i]);
        }
    }
    std::cout << std::flush;
}




//int watcher = -1;
//int esc_num = 0;

bool prev_was_esc = false;



void cO_lin::writeChar(char ch) {
    if (ch == 0x7F /*0x08*/) {
        // backspace
        //std::cout << "DEL:  " << 0x7F << std::flush;
        ch = 0x08;
    }
    //std::cout << ch << std::flush;
    //return;

    static std::string s;
    s.clear();
    decode_866(ch, s);
    static std::string debug;
    if (ch == '\033' || debug.length() > 0) {
        debug += ch;
        if (debug.length() >= 16) {
            if (false && debug[2] != '[' && debug[1] != '[') {
                int c = 0;
                int s = 0;
                std::ifstream in_file;
                in_file.open("out.txt");
                std::string contents((std::istreambuf_iterator<char>(in_file)), (std::istreambuf_iterator<char>()));
                in_file.close();

                std::ofstream out_file;
                out_file.open("out.txt");
                out_file << contents << std::endl;
                for (char ch : debug) {
                    ++s;
                    int code = (ch < 0) ? -ch : ch;
                    if (code < 32) {
                        out_file << c++ << " CHAR: int: " << std::dec << code << ", hex: " << std::hex << code << ", oct: " << std::oct << code << ";" << std::endl; 
                    } else {
                        out_file << c++ << " CHAR: '" << ch << "', int: " << std::dec << code << ", hex: " << std::hex << code << ", oct: " << std::oct << code << ";" << std::endl; 
                    }
                }
                if (s > 0) {
                    out_file << "\n\n" << std::endl;
                }
                out_file.close();
            }
            debug.clear();
        }
    }
    std::cout << s << std::flush;
}
