; Set 'non_atomic' annotation.
@.str = private unnamed_addr constant [10 x i8] c"nonatomic\00", section "llvm.metadata"
@llvm.global.annotations = appending global [1 x { ptr, ptr, ptr, i32, ptr }] [{ ptr, ptr, ptr, i32, ptr } { ptr @test, ptr @.str, ptr null, i32 39, ptr null }], section "llvm.metadata"

declare void @tick();

define i32 @test() {
    call void @tick();
    %x = add i32 0, 0
    call void @tick();
    %y = add i32 %x, 42
    ret i32 %y
}
