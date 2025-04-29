//
// Created by Troy Frever on 4/28/25.
//

#ifndef CUSTOM_EXCEPTIONS_H
#define CUSTOM_EXCEPTIONS_H

class CustomExceptionWithMessage: public std::exception {
public:
    explicit CustomExceptionWithMessage() {}
    const char* what() const noexcept override  {
        return message.c_str();
    }

protected:
    std::string message;
};

class MissingRequiredValueException: public CustomExceptionWithMessage {
public:
    explicit MissingRequiredValueException(const std::string& exp_message) {
        message = exp_message;
    }
};

class AllMissingValuesException : public CustomExceptionWithMessage {
public:
    explicit AllMissingValuesException(const std::string& vectorName) {
        message = "ERROR: All values in vector " + vectorName + " are missing!";
    }
};

class WrongLengthVectorException : public CustomExceptionWithMessage {
public:
    explicit WrongLengthVectorException(const std::string& vectorName){
        message = "ERROR: Length of vector " + vectorName + " does not match number of time steps!";
    }
};

#endif //CUSTOM_EXCEPTIONS_H
