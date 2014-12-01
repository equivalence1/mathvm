#ifndef DATA_HOLDER_H__
#define DATA_HOLDER_H__

#include <string>
#include <memory>

union DataHolder {
    int16_t stringId;
    int64_t intValue;
    double doubleValue;
};

#endif
