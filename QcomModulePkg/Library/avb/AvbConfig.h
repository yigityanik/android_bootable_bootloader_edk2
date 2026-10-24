/*
 * 0: tolerate AVB verification errors for local testing.
 * 1: enforce AVB verification while the device reports locked.
 * A permissive boot is reported as orange/unlocked to Android.
 */
#define AVB_HASH_VERIFICATION 0