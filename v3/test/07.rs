main()
eternal_sleep()

fn eternal_sleep() (
    loop(
        &status = usleep(0)
    )
)
fn putc(ch) (
    result = ext(ch, 6)
)
fn pen_down() (
    ext(11)
)
fn pen_up() (
    ext(12)
)
fn pen_color_set(x) (
    result = ext(x, 13)
)
fn pen_size_set(x) (
    result = ext(x, 14)
)
fn goto(x, y) (
    result = ext(x, y, 15)
)
fn allocate(dst, size) (
    &x = *3 - 2
    x = *x + size + 2
    dst = x
)
fn print_endl() (
    putc(10)
)
fn print_int(x) (
    &m = 1000000000
    if (x < 0) (
        putc(45)
        &x = x * -1
    ) else if (x == 0) (
        putc(48)
        return(0)
    )
    loop (
        if (x / m > 0) (
            break
        )
        &m = m / 10
    )
    loop (
        if (m == 0) (
            return(0)
        )
        putc(x / m % 10 + 48)
        &m = m / 10
    )
)
fn main() (
    loop (
        &mouse_down = *17
        &mouse_x = *18
        &mouse_y = *19
        if (mouse_down) (
            &status = pen_down()
        ) else (
            &status = pen_up()
        )
        &status = goto(mouse_x, mouse_y)
        &status = usleep(0)
    )
)