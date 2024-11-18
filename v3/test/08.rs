main()
eternal_sleep()

fn breakpoint() (
    ext(5)
)
fn putc(ch) (
    &result = ext(ch, 6)
)
fn erase_all() (
    ext(9)
)
fn stamp() (
    ext(10)
)
fn pen_down() (
    ext(11)
)
fn pen_up() (
    ext(12)
)
fn pen_color_set(x) (
    &result = ext(x, 13)
)
fn pen_size_set(x) (
    &result = ext(x, 14)
)
fn goto(x, y) (
    &result = ext(x, y, 15)
)

fn eternal_sleep() (
    loop (
        &status = usleep(0)
    )
)
fn _allocate(dst, size) (
    &x = *3 - 2
    x = *x + size + 2
    dst = x
)

fn swap(a, b) (
    &t = *a
    a = *b
    b = t
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
fn vec_init(v) (
    v = 0
)
fn vec_size(v) (
    return (*v)
)
fn vec_get(v, i) (
    return (*(v + i + 1))
)
fn vec_set(v, i, x) (
    v + i + 1 = x
)
fn vec_push(v, x) (
    v = *v + 1
    v + *v = x
)

fn main() (
    &button_capacity = 100
    _allocate(&button_x, button_capacity)
    _allocate(&button_y, button_capacity)
    _allocate(&button_dx, button_capacity)
    _allocate(&button_dy, button_capacity)
    &status = vec_init(button_x)
    &status = vec_init(button_y)
    &status = vec_init(button_dx)
    &status = vec_init(button_dy)
    &status = vec_push(button_x, 150)
    &status = vec_push(button_y, 50)
    &status = vec_push(button_dx, 50)
    &status = vec_push(button_dy, 20)
    &status = vec_push(button_x, -90)
    &status = vec_push(button_y, -20)
    &status = vec_push(button_dx, 60)
    &status = vec_push(button_dy, 15)


    loop (
        &mouse_isdown = *17
        &mouse_x = *18
        &mouse_y = *19

        &status = erase_all()
        &i = 0
        loop(
            if (i == vec_size(button_x)) (
                break
            )
            &x = vec_get(button_x, i)
            &y = vec_get(button_y, i)
            &dx = vec_get(button_dx, i)
            &dy = vec_get(button_dy, i)
            if (mouse_isdown && mouse_x > x - dx && mouse_x < x + dx && mouse_y > y - dy && mouse_y < y + dy) (
                &dx = dx + 4
                &dy = dy + 4
            )
            &status = goto(x - dx, y - dy)
            &status = pen_down()
            &status = goto(x - dx, y + dy)
            &status = goto(x + dx, y + dy)
            &status = goto(x + dx, y - dy)
            &status = goto(x - dx, y - dy)
            &status = pen_up()
            &i = i + 1
        )
        
        &status = usleep(0)
    )
)