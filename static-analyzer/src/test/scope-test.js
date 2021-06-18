function scopeOne() {
    var one = "I am in the scope created by `scopeOne()`";
    console.log(one);
  
    function scopeTwo() {
      var one = "I am creating a new `one` but leaving reference in `scopeOne()` alone.";
      console.log(one);
    }

    console.log(one);
    scopeTwo();
  }

scopeOne();
console.log(one);
if (typeof myscope !== 'undefined')
    console.log("myscope is defined.");