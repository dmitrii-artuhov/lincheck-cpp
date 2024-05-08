@.str.1 = private unnamed_addr constant [16 x i8] c"ltest_nonatomic\00", section "llvm.metadata"
@.str.2 = private unnamed_addr constant [17 x i8] c"ltesttarget_test\00", section "llvm.metadata"

@llvm.global.annotations = appending global [2 x { ptr, ptr, ptr, i32, ptr }] [{ ptr, ptr, ptr, i32, ptr } { ptr @test, ptr @.str.1, ptr null, i32 12, ptr null }, { ptr, ptr, ptr, i32, ptr } { ptr @test, ptr @.str.2, ptr null, i32 12, ptr null }], section "llvm.metadata"

declare void @CoroYield();

declare void @tick();

define i32 @test() {
    call void @tick();
    call void @CoroYield();
    %x = add i32 0, 0
    call void @tick();
    call void @CoroYield();
    %y = add i32 %x, 42
    ret i32 %y
}
