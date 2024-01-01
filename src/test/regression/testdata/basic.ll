declare void @tick();

define i32 @test() {
    call void @tick();
    %x = add i32 0, 0
    call void @tick();
    %y = add i32 %x, 42
    ret i32 %y
}
