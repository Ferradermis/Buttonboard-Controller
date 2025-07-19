echo("including params.scad");
include <./ISOThread.scad>
$fn=117;

//housing params (including threads)
thread_tolerance=0.25;
od_rim=35;
h_rim=4;
bevel_rim=2;
tpitch=2.0;
od_housing=29;
od_housing_minor=tpitch;
od_housing_wings=30;
id_housing=25;
h_housing=27;
w_housing_guide=2;

w_fin=1.2;
h_fin=6;

//button params
od_button=23.25;
id_button=21.50;
t_buttonface=3;
bevel_buttonface=2.5;
h_button=14.5;
h_button_travel=h_button+2;
w_button_guide=3.5;

//switch params
d_switch_key=6.37;
h_switch_key=3.5;
d_switch_neck=7;
d_switch_body=7.4;
d_switch_shoulder=12;



w_wire_notch=d_switch_body-2;



w_pixel_board=10.2;
w_pixel=7;
t_pixel=1;
t_pixel_unit=4.25;
w_pedestal=12.5;
h_pedestal=t_pixel_unit + 0.25;

h_nut=9;
d_nut=od_housing+10;
nut_sides=9;
h_nut_rim=3;

h_led_cap=10;
w_led_cap=w_pedestal+3;


module rcube(s,r=1){
    hull(){
        for(x=[r,s[0]-r])
            for(y=[r,s[1]-r])
                translate([x,y,0])
                    cylinder(r=r,h=s[2]);
    }
}
