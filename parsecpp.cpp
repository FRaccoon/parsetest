#include <iostream>
#include <sstream>
#include <string>
#include <functional>
#include <list>

template <typename T>
std::string toString(const std::list<T> &list) {
    std::stringstream ss;
    ss << "[";
    for (auto it = list.begin();
            it != list.end(); ++it) {
        if (it != list.begin()) ss << ",";
        ss << *it;
    }
    ss << "]";
    return ss.str();
}

template <typename T>
std::ostream &operator<<(std::ostream &cout, const std::list<T> &list) {
    return cout << toString(list);
}

class Source {
    const char *s;
    int line, col;
public:
    Source(const char *s) : s(s), line(1), col(1) {}
    char peek() {
        char ch = *s;
        if (!ch) throw ex("too short");
        return ch;
    }
    void next() {
        char ch = peek();
        if (ch == '\n') {
            ++line;
            col = 0;
        }
        ++s;
        ++col;
    }
    std::string ex(const std::string &e) {
        std::ostringstream oss;
        oss << "[line " << line << ", col " << col << "] " << e;
        if (*s) oss << ": '" << *s << "'";
        return oss.str();
    }
    bool operator==(const Source &src) const {
        return s == src.s;
    }
    bool operator!=(const Source &src) const {
        return !(*this == src);
    }
    friend std::ostream& operator << (std::ostream& cout, const Source& src) {
        cout << src.s;
        return cout;
    }
};

template <typename T>
using Parser = std::function<T (Source *)>;

template <typename T>
void parseTest(const Parser<T> &p, const Source &src) {
    Source s = src;
    try {
        std::cout << p(&s) << std::endl;
    } catch (const std::string &e) {
        std::cerr << e << std::endl;
    }
}

Parser<char> satisfy(const std::function<bool (char)> &f) {
    return [=](Source *s) {
        char ch = s->peek();
        if (!f(ch)) throw s->ex("not satisfy");
        s->next();
        return ch;
    };
}

auto anyChar = satisfy([](char) { return true; });

template <typename T1, typename T2>
Parser<std::string> operator+(const Parser<T1> &x, const Parser<T2> &y) {
    return [=](Source *s) {
        std::string ret;
        ret += x(s);
        ret += y(s);
        return ret;
    };
}

template <typename T>
Parser<std::string> operator*(int n, const Parser<T> &x) {
    return [=](Source *s) {
        std::string ret;
        for (int i = 0; i < n; ++i) {
            ret += x(s);
        }
        return ret;
    };
}

template <typename T>
Parser<std::string> operator*(const Parser<T> &x, int n) {
    return n * x;
}

template <typename T1, typename T2>
Parser<T1> operator<<(const Parser<T1> &p1, const Parser<T2> &p2) {
    return [=](Source *s) {
        T1 ret = p1(s);
        p2(s);
        return ret;
    };
}

template <typename T1, typename T2>
Parser<T2> operator>>(const Parser<T1> &p1, const Parser<T2> &p2) {
    return [=](Source *s) {
        p1(s);
        return p2(s);
    };
}

template <typename T1, typename T2>
Parser<T1> apply(const std::function <T1 (const T2 &)> &f, const Parser<T2> &p) {
    return [=](Source *s) {
        return f(p(s));
    };
}

template <typename T1, typename T2, typename T3>
Parser<std::function<T1 (const T2 &)>> apply(
        const std::function <T1 (const T2 &, const T3 &)> &f, const Parser<T2> &p) {
    return [=](Source *s) {
        T2 x = p(s);
        return [=](const T3 &y) {
            return f(x, y);
        };
    };
}

template <typename T>
Parser<T> operator-(const Parser<T> &p) {
    return apply<T, T>([=](T x) { return -x; }, p);
}

template <typename T>
Parser<std::string> many_(const Parser<T> &p) {
    return [=](Source *s) {
        std::string ret;
        try {
            for (;;) ret += p(s);
        } catch (const std::string &) {}
        return ret;
    };
}

Parser<std::string> many(const Parser<char> &p) {
    return many_(p);
}

Parser<std::string> many(const Parser<std::string> &p) {
    return many_(p);
}

template <typename T>
Parser<std::list<T>> many(const Parser<T> &p) {
    return [=](Source *s) {
        std::list<T> ret;
        try {
            for (;;) ret.push_back(p(s));
        } catch (const std::string &) {}
        return ret;
    };
}

template <typename T>
Parser<std::string> many1(const Parser<T> &p) {
    return p + many(p);
}

template <typename T>
const Parser<T> operator||(const Parser<T> &p1, const Parser<T> &p2) {
    return [=](Source *s) {
        T ret;
        Source bak = *s;
        try {
            ret = p1(s);
        } catch (const std::string &) {
            if (*s != bak) throw;
            ret = p2(s);
        }
        return ret;
    };
}

template <typename T>
Parser<T> tryp(const Parser<T> &p) {
    return [=](Source *s) {
        T ret;
        Source bak = *s;
        try {
            ret = p(s);
        } catch (const std::string &) {
            *s = bak;
            throw;
        }
        return ret;
    };
}

template <typename T>
Parser<T> left(const std::string &e) {
    return [=](Source *s) -> T {
        throw s->ex(e);
    };
}

Parser<char> left(const std::string &e) {
    return left<char>(e);
}

Parser<char> char1(char ch) {
    return satisfy([=](char c) { return c == ch; })
        || left(std::string("not char '") + ch + "'");
}

Parser<std::string> string(const std::string &str) {
    return [=](Source *s) {
        for (auto it = str.begin(); it != str.end(); ++it) {
            (char1(*it) || left("not string \"" + str + "\""))(s);
        }
        return str;
    };
}

bool isDigit   (char ch) { return '0' <= ch && ch <= '9'; }
bool isUpper   (char ch) { return 'A' <= ch && ch <= 'Z'; }
bool isLower   (char ch) { return 'a' <= ch && ch <= 'z'; }
bool isAlpha   (char ch) { return isUpper(ch) || isLower(ch); }
bool isAlphaNum(char ch) { return isAlpha(ch) || isDigit(ch); }
bool isLetter  (char ch) { return isAlpha(ch) || ch == '_';   }
bool isSpace   (char ch) { return ch == '\t'  || ch == ' ';   }

auto digit    = satisfy(isDigit   ) || left("not digit"   );
auto upper    = satisfy(isUpper   ) || left("not upper"   );
auto lower    = satisfy(isLower   ) || left("not lower"   );
auto alpha    = satisfy(isAlpha   ) || left("not alpha"   );
auto alphaNum = satisfy(isAlphaNum) || left("not alphaNum");
auto letter   = satisfy(isLetter  ) || left("not letter"  );
auto space    = satisfy(isSpace   ) || left("not space"   );

auto spaces = many(space);
