#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

static void pause_ms(long milliseconds) {
    struct timespec duration = {
        milliseconds / 1000, (milliseconds % 1000) * 1000000
    };
    while (nanosleep(&duration, &duration) < 0 && errno == EINTR) {}
}

static bool emit_event(int device, unsigned short type,
    unsigned short code, int value) {
    struct input_event event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.code = code;
    event.value = value;
    return write(device, &event, sizeof(event)) == sizeof(event);
}

static void sync_events(int device) {
    emit_event(device, EV_SYN, SYN_REPORT, 0);
}

static void point(int device, int x, int y) {
    emit_event(device, EV_ABS, ABS_X, x);
    emit_event(device, EV_ABS, ABS_Y, y);
    sync_events(device);
    emit_event(device, EV_REL, REL_X, -1);
    sync_events(device);
}

static void button(int device, bool pressed) {
    emit_event(device, EV_KEY, BTN_LEFT, pressed ? 1 : 0);
    sync_events(device);
}

static void click(int device, int x, int y) {
    point(device, x, y);
    pause_ms(120);
    button(device, true);
    pause_ms(50);
    button(device, false);
    pause_ms(120);
}

static void key(int device, unsigned short code) {
    emit_event(device, EV_KEY, code, 1);
    sync_events(device);
    pause_ms(45);
    emit_event(device, EV_KEY, code, 0);
    sync_events(device);
    pause_ms(120);
}

static void capture_fastword(const char *path) {
    char command[512];
    snprintf(command, sizeof(command),
        "grim -g '756,350 1048x768' '%s'", path);
    (void)system(command);
}

static unsigned short key_code(const char *name) {
    if (strcmp(name, "tab") == 0) return KEY_TAB;
    if (strcmp(name, "enter") == 0) return KEY_ENTER;
    if (strcmp(name, "space") == 0) return KEY_SPACE;
    if (strcmp(name, "escape") == 0) return KEY_ESC;
    if (strcmp(name, "left") == 0) return KEY_LEFT;
    if (strcmp(name, "right") == 0) return KEY_RIGHT;
    return KEY_RESERVED;
}

static int create_device(void) {
    int device = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (device < 0) return -1;
    ioctl(device, UI_SET_EVBIT, EV_KEY);
    ioctl(device, UI_SET_KEYBIT, BTN_LEFT);
    ioctl(device, UI_SET_KEYBIT, KEY_TAB);
    ioctl(device, UI_SET_KEYBIT, KEY_ENTER);
    ioctl(device, UI_SET_KEYBIT, KEY_SPACE);
    ioctl(device, UI_SET_KEYBIT, KEY_ESC);
    ioctl(device, UI_SET_KEYBIT, KEY_LEFT);
    ioctl(device, UI_SET_KEYBIT, KEY_RIGHT);
    ioctl(device, UI_SET_KEYBIT, KEY_W);
    ioctl(device, UI_SET_KEYBIT, KEY_A);
    ioctl(device, UI_SET_KEYBIT, KEY_S);
    ioctl(device, UI_SET_KEYBIT, KEY_D);
    ioctl(device, UI_SET_KEYBIT, KEY_LEFTCTRL);
    ioctl(device, UI_SET_EVBIT, EV_ABS);
    ioctl(device, UI_SET_ABSBIT, ABS_X);
    ioctl(device, UI_SET_ABSBIT, ABS_Y);
    ioctl(device, UI_SET_EVBIT, EV_REL);
    ioctl(device, UI_SET_RELBIT, REL_X);
    ioctl(device, UI_SET_RELBIT, REL_Y);
    ioctl(device, UI_SET_RELBIT, REL_WHEEL);

    struct uinput_setup setup;
    memset(&setup, 0, sizeof(setup));
    setup.id.bustype = BUS_USB;
    setup.id.vendor = 0x2e;
    setup.id.product = 0x4750;
    strcpy(setup.name, "Luma UI validation input");
    struct uinput_abs_setup absolute;
    memset(&absolute, 0, sizeof(absolute));
    absolute.code = ABS_X;
    absolute.absinfo.maximum = 3839;
    if (ioctl(device, UI_ABS_SETUP, &absolute) < 0) return -1;
    absolute.code = ABS_Y;
    absolute.absinfo.maximum = 2159;
    if (ioctl(device, UI_ABS_SETUP, &absolute) < 0 ||
        ioctl(device, UI_DEV_SETUP, &setup) < 0 ||
        ioctl(device, UI_DEV_CREATE) < 0) return -1;
    pause_ms(500);
    return device;
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    int device = create_device();
    if (device < 0) return 2;

    if (strcmp(argv[1], "move") == 0 && argc == 4) {
        point(device, atoi(argv[2]), atoi(argv[3]));
        pause_ms(500);
    } else if (strcmp(argv[1], "click") == 0 && argc == 4) {
        click(device, atoi(argv[2]), atoi(argv[3]));
    } else if (strcmp(argv[1], "drag") == 0 && argc >= 6 && argc <= 9) {
        int x = atoi(argv[2]), y = atoi(argv[3]);
        int end_x = atoi(argv[4]), end_y = atoi(argv[5]);
        int steps = argc >= 8 ? atoi(argv[7]) : 30;
        long interval_ms = argc >= 9 ? atol(argv[8]) : 20;
        if (steps < 1 || steps > 10000 || interval_ms < 1 || interval_ms > 1000) return 3;
        point(device, x, y);
        pause_ms(150);
        button(device, true);
        for (int step = 1; step <= steps; ++step) {
            point(device, x + (end_x - x) * step / steps,
                y + (end_y - y) * step / steps);
            pause_ms(interval_ms);
        }
        puts("holding pointer down");
        fflush(stdout);
        pause_ms(argc >= 7 ? atol(argv[6]) : 500);
        button(device, false);
        pause_ms(150);
    } else if (strcmp(argv[1], "active") == 0 && argc == 5) {
        point(device, atoi(argv[2]), atoi(argv[3]));
        pause_ms(150);
        button(device, true);
        pause_ms(atol(argv[4]));
        button(device, false);
    } else if (strcmp(argv[1], "key") == 0 && argc == 3) {
        unsigned short code = key_code(argv[2]);
        if (code == KEY_RESERVED) return 3;
        key(device, code);
    } else if (strcmp(argv[1], "fastword-review") == 0 && argc == 6) {
        int start_x = atoi(argv[2]);
        int start_y = atoi(argv[3]);
        int remember_x = atoi(argv[4]);
        int remember_y = atoi(argv[5]);
        pause_ms(1000);
        click(device, start_x, start_y);
        puts("started");
        fflush(stdout);
        pause_ms(2500);
        click(device, remember_x, remember_y);
        puts("review");
        fflush(stdout);
        pause_ms(3000);
        key(device, KEY_LEFT);
        puts("left");
        fflush(stdout);
        pause_ms(3000);
        key(device, KEY_RIGHT);
        puts("right");
        fflush(stdout);
        pause_ms(3000);
    } else if (strcmp(argv[1], "fastword-review-left") == 0 && argc == 6) {
        int start_x = atoi(argv[2]);
        int start_y = atoi(argv[3]);
        int remember_x = atoi(argv[4]);
        int remember_y = atoi(argv[5]);
        pause_ms(1000);
        click(device, start_x, start_y);
        pause_ms(2500);
        click(device, remember_x, remember_y);
        puts("review");
        fflush(stdout);
        pause_ms(10000);
        key(device, KEY_LEFT);
        puts("left");
        fflush(stdout);
        pause_ms(10000);
    } else if (strcmp(argv[1], "fastword-review-hold") == 0 && argc == 6) {
        int start_x = atoi(argv[2]);
        int start_y = atoi(argv[3]);
        int remember_x = atoi(argv[4]);
        int remember_y = atoi(argv[5]);
        pause_ms(1000);
        click(device, start_x, start_y);
        pause_ms(2500);
        click(device, remember_x, remember_y);
        puts("review");
        fflush(stdout);
        pause_ms(20000);
    } else if (strcmp(argv[1], "fastword-proof") == 0 && argc == 6) {
        int start_x = atoi(argv[2]);
        int start_y = atoi(argv[3]);
        int remember_x = atoi(argv[4]);
        int remember_y = atoi(argv[5]);
        pause_ms(1000);
        click(device, start_x, start_y);
        pause_ms(2500);
        click(device, remember_x, remember_y);
        pause_ms(500);
        capture_fastword("/tmp/fastword-proof-current.png");
        key(device, KEY_LEFT);
        pause_ms(500);
        capture_fastword("/tmp/fastword-proof-left.png");
        key(device, KEY_RIGHT);
        pause_ms(500);
        capture_fastword("/tmp/fastword-proof-right.png");
    } else if (strcmp(argv[1], "soak") == 0 && argc == 9) {
        long seconds = atol(argv[2]);
        int x[3] = {atoi(argv[3]), atoi(argv[5]), atoi(argv[7])};
        int y[3] = {atoi(argv[4]), atoi(argv[6]), atoi(argv[8])};
        time_t end = time(NULL) + seconds;
        int cycle = 0;
        while (time(NULL) < end) {
            int target = cycle % 3;
            point(device, x[target], y[target]);
            pause_ms(100);
            click(device, x[target], y[target]);
            key(device, KEY_TAB);
            key(device, cycle % 2 == 0 ? KEY_ENTER : KEY_SPACE);
            emit_event(device, EV_REL, REL_WHEEL, cycle % 2 == 0 ? 1 : -1);
            sync_events(device);
            pause_ms(120);
            cycle++;
        }
    } else {
        ioctl(device, UI_DEV_DESTROY);
        close(device);
        return 1;
    }

    ioctl(device, UI_DEV_DESTROY);
    close(device);
    return 0;
}
