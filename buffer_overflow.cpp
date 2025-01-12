#include <stdio.h>
#include <string.h>
#include <stdlib.h>
// Global counters for function calls
int processData_call_count = 0;
int combineData_call_count = 0;
int modifySize_call_count = 0;
int adjustValue_call_count = 0;
int amplifyValue_call_count = 0;
int reduceValue_call_count = 0;
int riskyFunction_call_count = 0;
int buffer_overflow_count = 0;

// Function declarations
void processData(int initialSize);
int combineData(int size, int factor);
int modifySize(int size);
int adjustValue(int value)
int amplifyValue(int value);
int reduceValue(int value);
void riskyFunction(int finalSize);
void performOverflow(int size);

int main() {
    int size1 = 5;   // Initial safe size
    int size2 = 10;  // Potentially risky size
    int size3 = 15;  // Risky size

    // Data flow from main to the overflow point
    processData(size1); // Safe


    return 0;
}

// Function to process data and pass it through multiple transformations
void processData(int initialSize) {
    processData_call_count++;


    // Initialize variables that will interact with the size
    int additionalValue = 2;
    int multipliedFactor = 3;
    int finalSize;

    // Modify size with various operations
    int modifiedSize = combineData(initialSize, additionalValue); // Add and pass the size


    modifiedSize = modifySize(modifiedSize); // Modify further


    // Combine size with other factors
    finalSize = combineData(modifiedSize, multipliedFactor); // Final modification


    riskyFunction(finalSize); // Eventually call the risky function
}

// Function to combine size with other variables
int combineData(int size, int factor) {
    combineData_call_count++;
    int result = size;
    // Adjust size with complex logic
    result += factor; // Add the factor to the size
    result *= 2;      // Double the size

    return result;
}



// Function to modify the size based on multiple conditions
int modifySize(int size) {
    modifySize_call_count++;
    int temp = adjustValue(size); // Adjust the value


    temp = amplifyValue(temp);    // Amplify the value


    temp = reduceValue(temp);     // Reduce the value


    return temp;
}

// Function to adjust the value
int adjustValue(int value) {
    adjustValue_call_count++;
    if (value % 3 == 0) {
        value += 7; // Adjust if value is a multiple of 3
    } else {
        value -= 3; // Adjust otherwise
    }

    return value;
}

// Function to amplify the value
int amplifyValue(int value) {
    amplifyValue_call_count++;
    value = (value * 4) + 10; // Amplify and add a constant

    return value;
}

// Function to reduce the value
int reduceValue(int value) {
    reduceValue_call_count++;
    if (value > 50) {
        value -= 15; // Reduce if value is large
    } else {
        value -= 5;  // Reduce less if value is smaller
    }
    return value;
}

// Function that contains a potential buffer overflow
void riskyFunction(int finalSize) {
    riskyFunction_call_count++;
    //do a malloc
    int* p = (int*)malloc(10 * sizeof(int));
    if (p == NULL) {
        printf("Memory allocation failed\n");
        return;
    }

    char smallBuffer[20];
    char largeBuffer[50] = "This is a very long string that can overflow the small buffer";
    // do strcpy
    //strcpy(smallBuffer, largeBuffer);
    // Dangerous use of memcpy causing buffer overflow
    //memcpy(smallBuffer, largeBuffer, 10); // Overflow if finalSize > 20

    // Perform some additional checks before calling performOverflow
    if (finalSize > 20) {
        // This will eventually cause buffer overflow if finalSize > 20
        performOverflow(finalSize);
    }
}

// Function that performs the actual buffer overflow
void performOverflow(int size) {
    char smallBuffer[20];
    char largeBuffer[50] = "This is a very long string that can overflow the small buffer";

    // Dangerous use of memcpy causing buffer overflow
    memcpy(smallBuffer, largeBuffer, size); // Overflow if size > 20
    smallBuffer[size] = '\0'; // Null-terminate the buffer
    buffer_overflow_count++;
}