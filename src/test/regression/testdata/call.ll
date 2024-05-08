@.str.1 = private unnamed_addr constant [16 x i8] c"ltest_nonatomic\00", section "llvm.metadata"
@.str.2 = private unnamed_addr constant [17 x i8] c"ltesttarget_test\00", section "llvm.metadata"

@llvm.global.annotations = appending global [4 x { ptr, ptr, ptr, i32, ptr }] [
    { ptr, ptr, ptr, i32, ptr } { ptr @test, ptr @.str.1, ptr null, i32 39, ptr null },
    { ptr, ptr, ptr, i32, ptr } { ptr @test, ptr @.str.2, ptr null, i32 39, ptr null },
    { ptr, ptr, ptr, i32, ptr } { ptr @foo, ptr @.str.1, ptr null, i32 39, ptr null },
    { ptr, ptr, ptr, i32, ptr } { ptr @bar, ptr @.str.1, ptr null, i32 39, ptr null }],
    section "llvm.metadata"

declare void @tick();

declare void @CoroYield();

define i32 @bar(i32 %x) {
    %cond = icmp sge i32 %x, 5
    call void @tick()
    call void @CoroYield()
    br i1 %cond, label %iftrue, label %iffalse
iffalse:
    %y = add i32 %x, 1
    %res = call i32 @bar(i32 %y)
    ret i32 %res
iftrue:
    ret i32 %x
}

define i32 @foo(i32 %a, i32 %b) {
    %c = add i32 %a, %b
    call void @tick()
    call void @CoroYield()
    %res = call i32 @bar(i32 %c)
    ret i32 %res
}

define i32 @test() {
    %1 = call i32 @foo(i32 1, i32 2)
    %2 = call i32 @foo(i32 10, i32 0)
    %sum = add i32 %1, %2
    ret i32 %sum
}
