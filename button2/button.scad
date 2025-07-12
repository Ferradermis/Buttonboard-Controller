include <./params.scad>


main();


module main(){
    difference(){
        union(){
            cylinder(d1=od_button - 2 * bevel_buttonface,d2=od_button,h=bevel_buttonface);
            translate([0,0,bevel_buttonface])
                cylinder(d=od_button,h=h_button-bevel_buttonface);
        }
        union(){
            translate([0,0,t_buttonface])
                cylinder(d=id_button,h=h_button);

            translate([-od_button/2,-w_button_guide/2,t_buttonface+1])
                cube([od_button,w_button_guide,h_button]);
            
            translate([0,0,h_button])
                rotate([0,90,0])
                    cylinder(d=10,h=od_button,center=true,$fn=3);

        }
    }

    translate([0,0,t_buttonface])
        led_pedestal();

}

module led_pedestal(){
    difference(){
        union(){
            translate([-w_pedestal/2,-w_pedestal/2,0])
                rcube([w_pedestal,w_pedestal,h_pedestal]);
        }
        union(){
            //pixel void
            translate([-w_pixel_board/2,-w_pixel/2,0])
                cube([w_pixel_board,w_pixel,h_pedestal]);

            translate([-w_pixel/2,-w_pixel_board/2,0])
                cube([w_pixel,w_pixel_board,h_pedestal]);

            translate([-w_pixel_board/2,-w_pixel_board/2,t_pixel])
                rcube([w_pixel_board,w_pixel_board,h_pedestal],2);

            translate([-w_pixel_board/2,-w_pixel_board/2,t_pixel])
                rcube([w_pixel_board,w_pixel_board,h_pedestal],2);

            translate([-w_pixel/2,0,t_pixel+1])
                cube([w_pixel,w_pixel_board,h_pedestal]);
        }

    }
}