/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#if BITS_PER_LONG < 64

static unsigned long long div_u64_u64(unsigned long long dividend,
				      unsigned long long divisor)
{
	unsigned long long result = 0;
	unsigned long long tmp_res, tmp_div;

	while (dividend >= divisor) {
		tmp_div = divisor << 1;
		tmp_res = 1;
		while (dividend >= tmp_div) {
			tmp_div <<= 1;
			tmp_res <<= 1;
		}
		dividend -= divisor * tmp_res;
		result += tmp_res;
	}
	return result;
}

#else /* BITS_PER_LONG >= 64 */

static inline unsigned long long div_u64_u64(unsigned long long dividend,
					     unsigned long long divisor)
{
	return dividend / divisor;
}

#endif /* BITS_PER_LONG >= 64 */

static char *uint2str(unsigned long long value, char *buf)
{
	unsigned long long digit, divisor = 10000000000000000000ULL;
	int first_digit = 1;

	while (divisor > 0) {
		digit = div_u64_u64(value, divisor);
		value -= digit * divisor;
		if (!first_digit || digit > 0 || divisor == 1) {
			*buf++ = '0' + digit;
			first_digit = 0;
		}
		divisor = div_u64_u64(divisor, 10);
	}

	return buf;
}

static char *int2str(long long value, char *buf)
{
	if (value < 0) {
		*buf++ = '-';
		value = -value;
	}
	return uint2str(value, buf);
}

static char *hex2str(unsigned long long value, char *buf,
		     unsigned long long leading_zero_mask)
{
	const char hexdigit[] = "0123456789abcdef";
	unsigned long long digit, divisor = 0x1000000000000000ULL;
	int first_digit = 1;

	while (divisor > 0) {
		digit = div_u64_u64(value, divisor);
		value -= digit * divisor;
		if (!first_digit || digit > 0 || divisor == 1 ||
		    divisor & leading_zero_mask) {
			*buf++ = hexdigit[digit];
			first_digit = 0;
		}
		divisor >>= 4;
	}

	return buf;
}

static char *align(char *p1, char *p0, unsigned long width)
{
	unsigned int n;

	if (p1 - p0 >= width)
		return p1;

	for (n = 1; p1 - n >= p0; n++)
		*(p0 + width - n) = *(p1 - n);
	memset(p0, ' ', width - (p1 - p0));
	return p0 + width;
}

static void __vprintk(const char *fmt, va_list ap)
{
	char buf[128];
	char *p, *p0;
	char c;
	unsigned long long v;
	unsigned int width;
	bool longmode;

	p = buf;

	while (1) {
		c = *fmt++;
		if (c == 0)
			break;
		else if (c == '%') {
			*p = 0;
			console_write(buf);
			p = buf;

			c = *fmt++;

			width = 0;
			p0 = p;
			while (c >= '0' && c <= '9') {
				width = width * 10 + c - '0';
				c = *fmt++;
				if (width >= sizeof(buf) - 1)
					width = 0;
			}

			longmode = false;
			if (c == 'l') {
				longmode = true;
				c = *fmt++;
			}

			switch (c) {
			case 'd':
				if (longmode)
					v = va_arg(ap, long);
				else
					v = va_arg(ap, int);
				p = int2str(v, p);
				p = align(p, p0, width);
				break;
			case 'p':
				*p++ = '0';
				*p++ = 'x';
				v = va_arg(ap, unsigned long);
				p = hex2str(v, p, (unsigned long)-1);
				break;
			case 's':
				console_write(va_arg(ap, const char *));
				break;
			case 'u':
				if (longmode)
					v = va_arg(ap, unsigned long);
				else
					v = va_arg(ap, unsigned int);
				p = uint2str(v, p);
				p = align(p, p0, width);
				break;
			case 'x':
				if (longmode)
					v = va_arg(ap, unsigned long);
				else
					v = va_arg(ap, unsigned int);
				p = hex2str(v, p, 0);
				p = align(p, p0, width);
				break;
			default:
				*p++ = '%';
				*p++ = c;
				break;
			}
		} else if (c == '\n') {
			*p++ = c;
			*p = 0;
			console_write(buf);
			p = buf;
			*p++ = '\r';
		} else
			*p++ = c;

		if (p >= &buf[sizeof(buf) - 1]) {
			*p = 0;
			console_write(buf);
			p = buf;
		}
	}

	*p = 0;
	console_write(buf);
}
