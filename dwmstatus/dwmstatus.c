#define _DEFAULT_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <X11/Xlib.h>

char *tzaus = "Australia/Sydney";

static Display *dpy;

char *
smprintf(char *fmt, ...)
{
	va_list fmtargs;
	char *ret;
	int len;

	va_start(fmtargs, fmt);
	len = vsnprintf(NULL, 0, fmt, fmtargs);
	va_end(fmtargs);

	ret = malloc(++len);
	if (ret == NULL) {
		perror("malloc");
		exit(1);
	}

	va_start(fmtargs, fmt);
	vsnprintf(ret, len, fmt, fmtargs);
	va_end(fmtargs);

	return ret;
}

char *
readfile(char *base, char *file)
{
	char *path, line[513];
	FILE *fd;

	memset(line, 0, sizeof(line));

	path = smprintf("%s/%s", base, file);
	fd = fopen(path, "r");
	if (fd == NULL)
		return NULL;
	free(path);

	if (fgets(line, sizeof(line)-1, fd) == NULL)
		return NULL;
	fclose(fd);

	return smprintf("%s", line);
}

char *
getbattery(char *base)
{
	char *co;
	int descap, remcap;
	char charging;

	descap = -1;
	remcap = -1;

	co = readfile(base, "present");
	if (co == NULL || co[0] != '1') {
		if (co != NULL) free(co);
		return smprintf("???");
	}
	free(co);

	co = readfile(base, "charge_full_design");
	if (co == NULL) {
		co = readfile(base, "energy_full_design");
		if (co == NULL)
			return smprintf("");
	}
	sscanf(co, "%d", &descap);
	free(co);

	co = readfile(base, "status");
	if (co) {
		if(0 == strncmp("Discharging", co, 11)) {
			charging = '-';
		} else {
			charging = '+';
		}
		free(co);
	} 
	co = readfile(base, "charge_now");
	if (co == NULL) {
		co = readfile(base, "energy_now");
		if (co == NULL)
			return smprintf("");
	}
	sscanf(co, "%d", &remcap);
	free(co);

	if (remcap < 0 || descap < 0)
		return smprintf("invalid");

	return smprintf("%c%.0f", charging, ((float)remcap / (float)descap) * 100);
}

void
settz(char *tzname)
{
	setenv("TZ", tzname, 1);
}

char *
mktimes(char *fmt, char *tzname)
{
	char buf[129];
	time_t tim;
	struct tm *timtm;

	memset(buf, 0, sizeof(buf));
	settz(tzname);
	tim = time(NULL);
	timtm = localtime(&tim);
	if (timtm == NULL) {
		perror("localtime");
		exit(1);
	}

	if (!strftime(buf, sizeof(buf)-1, fmt, timtm)) {
		fprintf(stderr, "strftime == 0\n");
		exit(1);
	}

	return smprintf("%s", buf);
}

void
setstatus(char *str)
{
	XStoreName(dpy, DefaultRootWindow(dpy), str);
	XSync(dpy, False);
}

char *
loadavg(void)
{
	double avgs[3];

	if (getloadavg(avgs, 3) < 0) {
		perror("getloadavg");
		exit(1);
	}

	return smprintf("%.2f %.2f %.2f", avgs[0], avgs[1], avgs[2]);
}

// #define DEBUG
#define BAT_PATH "/sys/class/power_supply/BAT0/"
int
main(void)
{
	char *status;
	char *tmau;
	char *bat;

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return 1;
	}
#ifdef DEBUG
	int i = 0;
#endif

	for (;;sleep(10)) {
		tmau = mktimes("%a %b %d %H:%M", tzaus);
		bat = getbattery(BAT_PATH);
		status = smprintf("%s Battery: %s%%", tmau, bat);
#ifdef DEBUG
        puts(status);
	i++;
	if (i > 10) {
		goto done;
		break;
	}
#else
		setstatus(status);
#endif
#ifdef DEBUG
done:
#endif
		free(bat);
		free(tmau);
		free(status);
	}

	XCloseDisplay(dpy);

	return 0;
}

