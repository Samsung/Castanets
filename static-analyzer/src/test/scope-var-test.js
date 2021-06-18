var myglobal = "My Global";
{
    var myscope = "My Scope";

    function myfunc(p) {
        return p + " " + myglobal + " " + myscope;
    }
}

console.log(myfunc("Test"));
if (typeof myscope !== 'undefined')
    console.log("myscope is defined.");