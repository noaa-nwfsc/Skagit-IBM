//
// Created by Troy Frever on 4/28/25.
//

#ifndef CUSTOM_EXCEPTIONS_H
#define CUSTOM_EXCEPTIONS_H

class VectorExceptionWithMessage: public std::exception {
public:
    explicit VectorExceptionWithMessage() {}
    const char* what() const noexcept override  {
        return message.c_str();
    }

protected:
    std::string message;
};

class AllMissingValuesException : public VectorExceptionWithMessage {
public:
    explicit AllMissingValuesException(const std::string& vectorName) {
        message = "ERROR: All values in vector " + vectorName + " are missing!";
    }
};

class WrongLengthVectorException : public VectorExceptionWithMessage {
public:
    explicit WrongLengthVectorException(const std::string& vectorName){
        message = "ERROR: Length of vector " + vectorName + " does not match number of time steps!";
    }
};

#endif //CUSTOM_EXCEPTIONS_H
