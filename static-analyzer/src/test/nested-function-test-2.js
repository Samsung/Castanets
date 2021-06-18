var global_var;
function outer() {
    let var1;
    global_var = 1;
    function inner_1() {
    let var2;
    function inner_2() {
        let var3;
        console.log(var2);
    }
    return var1;
    }
}
