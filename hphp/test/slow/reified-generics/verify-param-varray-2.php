<?hh

function f<reify T>(T $x) :mixed{ echo "yes\n"; }
<<__EntryPoint>> function main(): void {
f<varray<int>>(varray[]);
f<varray<int>>(darray[]);
f<varray<int>>(vec[]);
f<varray<int>>(dict[]);
f<varray<int>>(keyset[]);
}
