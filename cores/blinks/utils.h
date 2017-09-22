/*
 * utils.h
 *
 * Created: 7/14/2017 1:50:04 PM
 *  Author: josh
 */ 


#ifndef UTILS_H_
#define UTILS_H_

// Bit manipulation macros
#define SBI(x,b) (x|= (1<<b))           // Set bit in IO reg
#define CBI(x,b) (x&=~(1<<b))           // Clear bit in IO reg
#define TBI(x,b) (x&(1<<b))             // Test bit in IO reg

typedef struct {
    uint8_t bytes[9] ;
} utils_serialno_t;

// Returns the device's unique 8-byte serial number

utils_serialno_t const *utils_serialno(void);

#endif /* UTILS_H_ */