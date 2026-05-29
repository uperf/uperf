#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

/*
 * We simulate the vulnerable password callback pattern from src/ssl.c:432
 * The OpenSSL password callback signature is:
 *   int pem_password_cb(char *buf, int size, int rwflag, void *userdata)
 * where 'size' is the maximum buffer size.
 *
 * The vulnerable code does: strcpy(buf, pass)
 * The safe version must: use strncpy or check length before copying.
 *
 * Invariant: Buffer reads never exceed the declared length.
 * The password callback must NEVER write more than 'size' bytes into buf.
 */

#define SAFE_BUF_SIZE 1024

/*
 * Safe implementation of the password callback (what the code SHOULD do).
 * Returns -1 if password exceeds buffer, or the length written.
 */
static int safe_password_callback(char *buf, int size, const char *pass)
{
    if (!buf || !pass || size <= 0)
        return -1;

    size_t pass_len = strlen(pass);

    /* Invariant: must not copy more than size-1 chars (leave room for NUL) */
    if ((int)pass_len >= size) {
        /* Reject oversized password - do not overflow */
        return -1;
    }

    strncpy(buf, pass, (size_t)(size - 1));
    buf[size - 1] = '\0';

    return (int)strlen(buf);
}

/*
 * Canary-protected buffer test harness.
 * Places canary bytes before and after the buffer to detect overflows.
 */
#define CANARY_SIZE 64
#define CANARY_BYTE 0xAB
#define TEST_BUF_SIZE 64  /* Simulated fixed-size stack buffer from ssl.c */

typedef struct {
    uint8_t pre_canary[CANARY_SIZE];
    char    buf[TEST_BUF_SIZE];
    uint8_t post_canary[CANARY_SIZE];
} canary_buffer_t;

static void init_canary_buffer(canary_buffer_t *cb)
{
    memset(cb->pre_canary,  CANARY_BYTE, CANARY_SIZE);
    memset(cb->buf,         0x00,        TEST_BUF_SIZE);
    memset(cb->post_canary, CANARY_BYTE, CANARY_SIZE);
}

static int check_canaries(const canary_buffer_t *cb)
{
    for (int i = 0; i < CANARY_SIZE; i++) {
        if (cb->pre_canary[i]  != CANARY_BYTE) return 0;
        if (cb->post_canary[i] != CANARY_BYTE) return 0;
    }
    return 1;
}

START_TEST(test_password_callback_no_buffer_overflow)
{
    /* Invariant: Buffer reads/writes never exceed the declared buffer length.
     * Any password string longer than or equal to TEST_BUF_SIZE must be
     * rejected or truncated — never cause an out-of-bounds write. */

    const char *payloads[] = {
        /* Exact boundary */
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", /* 64 chars == TEST_BUF_SIZE */

        /* 2x oversized */
        "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
        "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB", /* 128 chars */

        /* 10x oversized */
        "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC"
        "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC"
        "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC"
        "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC"
        "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC"
        "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC"
        "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC"
        "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC"
        "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC"
        "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC", /* 640 chars */

        /* SQL injection style */
        "' OR '1'='1'; DROP TABLE users; --AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",

        /* Format string attack */
        "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",

        /* Null bytes embedded (C string stops at first null, but tests boundary) */
        "password\x00AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",

        /* Unicode/high-byte attack */
        "\xff\xfe\xff\xfe\xff\xfe\xff\xfe\xff\xfe\xff\xfe\xff\xfe\xff\xfe"
        "\xff\xfe\xff\xfe\xff\xfe\xff\xfe\xff\xfe\xff\xfe\xff\xfe\xff\xfe"
        "\xff\xfe\xff\xfe\xff\xfe\xff\xfe\xff\xfe\xff\xfe\xff\xfe\xff\xfe"
        "\xff\xfe\xff\xfe\xff\xfe\xff\xfe\xff\xfe\xff\xfe\xff\xfe\xff\xfe"
        "\xff\xfe\xff\xfe\xff\xfe\xff\xfe\xff\xfe\xff\xfe\xff\xfe\xff\xfe",

        /* Newline/carriage return injection */
        "pass\r\nword\r\nAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",

        /* Very long password (1024 chars) */
        "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
        "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
        "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
        "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
        "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
        "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
        "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
        "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
        "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
        "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
        "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
        "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
        "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
        "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
        "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
        "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD",

        /* Empty string (edge case) */
        "",

        /* Single char (valid) */
        "x",

        /* Exactly one less than buffer size (valid) */
        "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE", /* 63 chars */
    };

    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        canary_buffer_t cb;
        init_canary_buffer(&cb);

        const char *pass = payloads[i];
        size_t pass_len = strlen(pass);

        int result = safe_password_callback(cb.buf, TEST_BUF_SIZE, pass);

        /* Invariant 1: Canaries must be intact — no buffer overflow occurred */
        ck_assert_msg(check_canaries(&cb),
            "SECURITY VIOLATION: Buffer overflow detected for payload[%d] "
            "(length=%zu). Canary bytes were overwritten!", i, pass_len);

        /* Invariant 2: If password fits, it must be accepted and correctly returned */
        if ((int)pass_len < TEST_BUF_SIZE) {
            ck_assert_msg(result >= 0,
                "Valid password of length %zu was incorrectly rejected (payload[%d])",
                pass_len, i);
            ck_assert_msg(result == (int)pass_len,
                "Returned length %d does not match password length %zu (payload[%d])",
                result, pass_len, i);
        }

        /* Invariant 3: If password is too long, it must be rejected (result < 0) */
        if ((int)pass_len >= TEST_BUF_SIZE) {
            ck_assert_msg(result < 0,
                "SECURITY VIOLATION: Oversized password (length=%zu) was accepted "
                "without rejection (payload[%d]). Expected rejection (result<0), got %d",
                pass_len, i, result);
        }

        /* Invariant 4: Buffer must always be NUL-terminated if data was written */
        if (result > 0) {
            ck_assert_msg(cb.buf[TEST_BUF_SIZE - 1] == '\0' ||
                          cb.buf[result] == '\0',
                "Buffer is not NUL-terminated after write (payload[%d])", i);
        }

        /* Invariant 5: Written length must never exceed buffer size */
        if (result > 0) {
            ck_assert_msg(result < TEST_BUF_SIZE,
                "SECURITY VIOLATION: Written length %d >= buffer size %d (payload[%d])",
                result, TEST_BUF_SIZE, i);
        }

        /* Invariant 6: Actual string in buffer must not exceed buffer bounds */
        if (result > 0) {
            size_t actual_len = strnlen(cb.buf, TEST_BUF_SIZE);
            ck_assert_msg((int)actual_len < TEST_BUF_SIZE,
                "SECURITY VIOLATION: String in buffer has length %zu >= buffer size %d "
                "(payload[%d])", actual_len, TEST_BUF_SIZE, i);
        }
    }
}
END_TEST

START_TEST(test_password_callback_null_inputs)
{
    /* Invariant: Null inputs must be handled gracefully without crashes */
    canary_buffer_t cb;
    init_canary_buffer(&cb);

    int result;

    /* NULL buffer */
    result = safe_password_callback(NULL, TEST_BUF_SIZE, "password");
    ck_assert_msg(result < 0, "NULL buffer should return error");

    /* NULL password */
    result = safe_password_callback(cb.buf, TEST_BUF_SIZE, NULL);
    ck_assert_msg(result < 0, "NULL password should return error");

    /* Zero size */
    result = safe_password_callback(cb.buf, 0, "password");
    ck_assert_msg(result < 0, "Zero size should return error");

    /* Negative size */
    result = safe_password_callback(cb.buf, -1, "password");
    ck_assert_msg(result < 0, "Negative size should return error");

    /* Canaries must still be intact */
    ck_assert_msg(check_canaries(&cb),
        "SECURITY VIOLATION: Canary bytes corrupted during null input handling");
}
END_TEST

START_TEST(test_password_callback_dynamic_oversized)
{
    /* Invariant: Dynamically generated oversized passwords must never overflow */
    canary_buffer_t cb;

    /* Test with 2x, 5x, 10x, 100x buffer size */
    int multipliers[] = {2, 5, 10, 100};
    int num_multipliers = sizeof(multipliers) / sizeof(multipliers[0]);

    for (int m = 0; m < num_multipliers; m++) {
        int oversized_len = TEST_BUF_SIZE * multipliers[m];
        char *oversized_pass = (char *)malloc((size_t)(oversized_len + 1));
        ck_assert_msg(oversized_pass != NULL, "malloc failed");

        memset(oversized_pass, 'A', (size_t)oversized_len);
        oversized_pass[oversized_len] = '\0';

        init_canary_buffer(&cb);

        int result = safe_password_callback(cb.buf, TEST_BUF_SIZE, oversized_pass);

        /* Invariant: Canaries must be intact */
        ck_assert_msg(check_canaries(&cb),
            "SECURITY VIOLATION: Buffer overflow detected for %dx oversized password "
            "(length=%d). Canary bytes were overwritten!", multipliers[m], oversized_len);

        /* Invariant: Overs