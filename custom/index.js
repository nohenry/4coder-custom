function pred(character){
    let predicate = new Array(32).fill(0);
    predicate[Math.floor(character/8)] = (1 << (character%8));
    return predicate;
}

function print(predicate) {
    let str = '';
    for (let i = 0; i < predicate.length; i += 1) {
        if (i % 8 == 0 && i != 0) str += '\n';
        str += predicate[i].toString().padStart(5);
        if (i % 8 != 7) str += ', ';
    }
    console.log(str)
}

function pred_or(a, b) {
    let predicate = new Array(32);
    for (let i = 0; i < a.length; i++) {
        predicate[i] = a[i] | b[i];
    }
    return predicate;
}

String.prototype.char = function () {
    return this.charCodeAt(0)
}

let a = pred('a'.char());
a = pred_or(a, pred('b'.char())) 
// for (let i = 'b'.char(); i <= 'z'.char(); i++) {
//     a = pred_or(a, pred(i)) 
// }
// for (let i = 'A'.char(); i <= 'Z'.char(); i++) {
//     a = pred_or(a, pred(i)) 
// }
// for (let i = '0'.char(); i <= '9'.char(); i++) {
//     a = pred_or(a, pred(i)) 
// }

print(a)