
define i32 @func() {
b1:
    %x = add i32 0, 1
    br label %b2

b2:
    br label %ret_block

ret_block:
    ret i32 %x
}

define i32 @main() {
    %x = call i32 @func()
    ret i32 %x
}

