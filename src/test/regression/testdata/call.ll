; Set 'non_atomic' annotation.
@.str = private unnamed_addr constant [10 x i8] c"nonatomic\00", section "llvm.metadata"
@llvm.global.annotations = appending global [3 x { ptr, ptr, ptr, i32, ptr }] [
    { ptr, ptr, ptr, i32, ptr } { ptr @test, ptr @.str, ptr null, i32 39, ptr null },
    { ptr, ptr, ptr, i32, ptr } { ptr @foo, ptr @.str, ptr null, i32 39, ptr null },
    { ptr, ptr, ptr, i32, ptr } { ptr @bar, ptr @.str, ptr null, i32 39, ptr null }],
    section "llvm.metadata"

declare void @tick();

define i32 @bar(i32 %x) {
    %cond = icmp sge i32 %x, 5
    call void @tick()
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
    %res = call i32 @bar(i32 %c)
    ret i32 %res
}

define i32 @test() {
    %1 = call i32 @foo(i32 1, i32 2)
    %2 = call i32 @foo(i32 10, i32 0)
    %sum = add i32 %1, %2
    ret i32 %sum
}
