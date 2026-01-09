include <./params.scad>


main();

module main(){
    intersection(){
        iso_nut(m=od_housing + thread_tolerance ,p=tpitch,w=8);
        cylinder(d=od_housing + 10,h=8,$fn=19);
    }
}