#ifndef LAENNEC_SELFTEST_H
#define LAENNEC_SELFTEST_H

/* Runs the features + classifier on the test frames stored in flash
 * (laennec_test_frames.h), times each one with the CPU cycle counter,
 * checks the results against the PC, and prints a report over USB serial.
 * Call once, after the serial port is set up. */
void laennec_selftest(void);

#endif /* LAENNEC_SELFTEST_H */
