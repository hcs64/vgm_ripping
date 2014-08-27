#include <stdio.h>

/*#define EXE "../zwsrt"*/
#define EXE "zwsrt.exe"

void conv(const char * srt_name, const char * ssd_name,int num) {
    char numbuf[10];
    int i;
    for (i=0;i<num;i++) {
        if (num<=99) sprintf(numbuf,"%02d",i);
        else sprintf(numbuf,"%03d",i);

        printf(EXE " %s %d %s_%s.aif.ssd %s_%s.zwdsp\n",srt_name,i,ssd_name,numbuf,ssd_name,numbuf);
    }
}

void conv1(const char * srt_name, int i, const char * ssd_name) {
    printf(EXE " %s %d %s.ssd %s.zwdsp\n",srt_name,i,ssd_name,ssd_name);
}

int main(void) {
    conv("EV00.srt","ev00",23);
    conv("EV01.srt","ev01",54);
    conv("EV02.srt","ev02",3);
    conv("EV03.srt","ev03",9);
    conv("EV04.srt","ev04",16);
    conv("EV05.srt","ev05",7);
    conv("EV06.srt","ev06",5);
    conv("EV07.srt","ev07",16);
    conv("EV08.srt","ev08",20);
    conv("EV09.srt","ev09",7);
    conv("EV10.srt","ev10",8);
    conv("EV11.srt","ev11",14);
    conv("EV12.srt","ev12",29);
    conv("EV13.srt","ev13",26);
    conv("EV14.srt","ev14",14);
    conv("EV15.srt","ev15",28);
    conv("EV16.srt","ev16",6);
    conv("EV17.srt","ev17",11);
    conv("EV18.srt","ev18",19);
    conv("EV19.srt","ev19",38);
    conv("EV20.srt","ev20",19);
    conv("EV21.srt","ev21",19);

    conv("BA1.srt","ba1",110);
    conv("BA2.srt","ba2",110);
    conv("GA.srt","ga",110);
    conv("HI.srt","hi",110);
    conv("JO.srt","jo",110);
    conv("OB.srt","ob",110);
    conv("OK.srt","ok",110);
    conv("OY.srt","oy",110);
    conv("PA.srt","pa",110);
    conv("RO.srt","ro",111);
    conv("SA.srt","sa",110);
    conv("SE.srt","se",111);
    conv("WI.srt","wi",110);
    conv("ZA.srt","za",110);

    conv1("bgm_DEMO.srt",0,"st_s_ev00");
    conv1("bgm_DEMO.srt",1,"st_s_dm01");
    /* another possibility
    conv1("bgm_DEMO.srt",1,"st_s_barbaros");*/
    conv1("bgm_DEMO.srt",2,"st_s_dm02");
    conv1("bgm_DEMO.srt",3,"st_s_ev02a");
    conv1("bgm_DEMO.srt",4,"st_s_ev02b");
    conv1("bgm_DEMO.srt",5,"st_s_ev00_SKIP");
    /* another poss
    conv1("bgm_DEMO.srt,5,"st_s05_00c");*/
    conv1("bgm_DEMO.srt",6,"st_s_dm03");
    conv1("bgm_DEMO.srt",7,"st_s_dm04");
    conv1("bgm_DEMO.srt",8,"st_s_dm05");
    conv1("bgm_DEMO.srt",9,"st_s_dm06");
    conv1("bgm_DEMO.srt",10,"st_s_ev_end");
    conv1("bgm_DEMO.srt",11,"st_s_dm07");
    conv1("bgm_DEMO.srt",12,"st_s_ev03a");
    conv1("bgm_DEMO.srt",13,"st_s_ev03b");
    conv1("bgm_DEMO.srt",14,"st_s_ev04a");
    conv1("bgm_DEMO.srt",15,"st_s_ev04b");
    conv1("bgm_DEMO.srt",16,"st_s_ev05a");
    conv1("bgm_DEMO.srt",17,"st_s_ev06a");
    conv1("bgm_DEMO.srt",18,"st_s_ev06b");
    conv1("bgm_DEMO.srt",19,"st_s_ev11a");
    /* another poss
    conv1("bgm_DEMO.srt",19,"st_s08_00b");*/
    conv1("bgm_DEMO.srt",20,"st_s_ev16a");
    conv1("bgm_DEMO.srt",21,"st_s_ev18a");
    conv1("bgm_DEMO.srt",22,"st_s_ev18b");
    conv1("bgm_DEMO.srt",23,"st_s_ev19a");
    conv1("bgm_DEMO.srt",24,"st_s_ev19b");
    conv1("bgm_DEMO.srt",25,"st_s_dm08");

    conv1("bgm_S01.srt",0,"st_s01_00a");
    conv1("bgm_S01.srt",1,"st_s01_00b");
    conv1("bgm_S01.srt",2,"st_s01_00c");
    conv1("bgm_S01.srt",3,"st_s01_01a");
    conv1("bgm_S01.srt",4,"st_s01_01b");
    conv1("bgm_S01.srt",5,"st_s01_02a");
    conv1("bgm_S01.srt",6,"st_s01_02b");
    conv1("bgm_S01.srt",7,"st_s01_02c");

    conv1("bgm_S02.srt",0,"st_s02_00");
    conv1("bgm_S02.srt",1,"st_s02_01s");
    conv1("bgm_S02.srt",2,"st_s02_01m");
    conv1("bgm_S02.srt",3,"st_s02_01q");
    conv1("bgm_S02.srt",4,"st_s02_02t");
    conv1("bgm_S02.srt",5,"st_s02k");
    conv1("bgm_S02.srt",6,"st_s02_02");
    conv1("bgm_S02.srt",7,"st_s02_02w1");
    conv1("bgm_S02.srt",8,"st_s02_02w2");
    conv1("bgm_S02.srt",9,"st_s02_bs");
    conv1("bgm_S02.srt",10,"st_s02k_bs");
    conv1("bgm_S02.srt",11,"st_s02_04o");
    conv1("bgm_S02.srt",12,"st_s02_04x");
    conv1("bgm_S02.srt",13,"st_s02_04xN");
    conv1("bgm_S02.srt",14,"st_s02_04o_SKIP");
    conv1("bgm_S02.srt",15,"st_s02_04x_SKIP");
    conv1("bgm_S02.srt",16,"st_s02_04oN");
    conv1("bgm_S02.srt",17,"st_s02_05a");
    conv1("bgm_S02.srt",18,"st_s02_02o");
    conv1("bgm_S02.srt",19,"st_s02_02o2");
    conv1("bgm_S02.srt",20,"st_s02_02i");
    conv1("bgm_S02.srt",21,"st_s02_00t");
    conv1("bgm_S02.srt",22,"st_s02_02w3");
    conv1("bgm_S02.srt",23,"st_s02_02w4");
    conv1("bgm_S02.srt",24,"st_s02_02w5");
    conv1("bgm_S02.srt",25,"st_s02_05b");

    conv1("bgm_S03.srt",0,"st_s03_00");
    conv1("bgm_S03.srt",1,"st_s03k");
    conv1("bgm_S03.srt",2,"st_s03_01a");
    conv1("bgm_S03.srt",3,"st_s03_02a");
    conv1("bgm_S03.srt",4,"st_s03_02b");
    conv1("bgm_S03.srt",5,"st_s03_bs");

    conv1("bgm_S04.srt",0,"st_s04_00");
    conv1("bgm_S04.srt",1,"st_s04k");
    conv1("bgm_S04.srt",2,"st_s04_02a");
    conv1("bgm_S04.srt",3,"st_s04_01o");
    conv1("bgm_S04.srt",4,"st_s04_01x_1");
    conv1("bgm_S04.srt",5,"st_s04_01x_2");
    conv1("bgm_S04.srt",6,"st_s04_01x_3");
    conv1("bgm_S04.srt",7,"st_s04_01h");
    conv1("bgm_S04.srt",8,"st_s04_bs");
    conv1("bgm_S04.srt",9,"st_s04k_bs");
    conv1("bgm_S04.srt",10,"st_s04_05a");

    conv1("bgm_S05.srt",0,"st_s05_00");
    conv1("bgm_S05.srt",1,"st_s05_00b");
    conv1("bgm_S05.srt",2,"st_s05k");
    conv1("bgm_S05.srt",3,"st_s05_00c");

    conv1("bgm_S06.srt",0,"st_s06_00");
    conv1("bgm_S06.srt",1,"st_s06k");
    conv1("bgm_S06.srt",2,"st_s06_03a");
    conv1("bgm_S06.srt",3,"st_s06_03b");
    conv1("bgm_S06.srt",4,"st_s06_bs");
    conv1("bgm_S06.srt",5,"st_s06k_bs");
    conv1("bgm_S06.srt",6,"st_s06_05a");
    conv1("bgm_S06.srt",7,"st_s06_05b");
    conv1("bgm_S06.srt",8,"st_s06_05c");

    conv1("bgm_S07.srt",0,"st_s07_00");
    conv1("bgm_S07.srt",1,"st_s07_00a");
    conv1("bgm_S07.srt",2,"st_s07_00b");

    conv1("bgm_S08.srt",0,"st_s08_00");
    conv1("bgm_S08.srt",1,"st_s08_01");
    conv1("bgm_S08.srt",2,"st_s08k");
    conv1("bgm_S08.srt",3,"st_s08_00a");
    conv1("bgm_S08.srt",4,"st_s08_00b");
    conv1("bgm_S08.srt",5,"st_s08_00c");
    conv1("bgm_S08.srt",6,"st_s08_00d");
    conv1("bgm_S08.srt",7,"st_s08_00e");
    conv1("bgm_S08.srt",8,"st_s08_00f");

    conv1("bgm_S09.srt",0,"01_st_s01_01b");
    conv1("bgm_S09.srt",1,"02_st_s01_00b");
    conv1("bgm_S09.srt",2,"03_st_s01_01a");
    conv1("bgm_S09.srt",3,"04_st_s01_02a");
    conv1("bgm_S09.srt",4,"05_st_s01_02b");
    conv1("bgm_S09.srt",5,"06_st_s02_00");
    conv1("bgm_S09.srt",6,"07_st_s02_01s");
    conv1("bgm_S09.srt",7,"08_st_s02_01m");
    conv1("bgm_S09.srt",8,"09_st_s02_01q");
    conv1("bgm_S09.srt",9,"10_st_s02_04o");
    conv1("bgm_S09.srt",10,"11_st_s02_02");
    conv1("bgm_S09.srt",11,"12_st_s02_bs");
    conv1("bgm_S09.srt",12,"13_st_s03_00");
    conv1("bgm_S09.srt",13,"14_st_s03_01a");
    conv1("bgm_S09.srt",14,"15_st_s03_bs");
    conv1("bgm_S09.srt",15,"16_st_s04_00");
    conv1("bgm_S09.srt",16,"17_st_s04_01o");
    conv1("bgm_S09.srt",17,"18_st_s04_02a");
    conv1("bgm_S09.srt",18,"19_st_s04_bs");
    conv1("bgm_S09.srt",19,"20_st_s05_00");
    conv1("bgm_S09.srt",20,"21_st_s06_00");
    conv1("bgm_S09.srt",21,"22_st_s06_bs");
    conv1("bgm_S09.srt",22,"23_st_s07_00a");
    conv1("bgm_S09.srt",23,"24_st_s08_00");
    conv1("bgm_S09.srt",24,"25_st_s_select");
    conv1("bgm_S09.srt",25,"26_st_s_ev00");
    conv1("bgm_S09.srt",26,"27_st_s_result");
    conv1("bgm_S09.srt",27,"28_st_s_usagi");
    conv1("bgm_S09.srt",28,"29_st_s_god");
    conv1("bgm_S09.srt",29,"30_st_s_kaizoku1");
    conv1("bgm_S09.srt",30,"31_st_s_kaizoku2");
    conv1("bgm_S09.srt",31,"32_st_s_kaizoku3");
    conv1("bgm_S09.srt",32,"33_st_s_kaizoku4");
    conv1("bgm_S09.srt",33,"34_st_s_kaizoku5");
    conv1("bgm_S09.srt",34,"35_st_s_kaizoku6");
    conv1("bgm_S09.srt",35,"36_st_s_kaizoku7");
    conv1("bgm_S09.srt",36,"37_st_s_kaizoku8");
    conv1("bgm_S09.srt",37,"38_st_s_kaizoku9");
    conv1("bgm_S09.srt",38,"39_st_s_thema_f");

    conv1("bgm_SYS.srt",0,"st_s_ajito");
    conv1("bgm_SYS.srt",1,"st_s_select");
    conv1("bgm_SYS.srt",2,"st_s_result");
    conv1("bgm_SYS.srt",3,"st_s_remocon");
    /* or 
    conv1("bgm_SYS.srt",3,"st_s_barbaros");*/
    conv1("bgm_SYS.srt",4,"st_s_usagi");
    conv1("bgm_SYS.srt",5,"st_j_item");
    conv1("bgm_SYS.srt",6,"st_j_clear");
    conv1("bgm_SYS.srt",7,"st_j_success");
    conv1("bgm_SYS.srt",8,"st_j_fail");
    conv1("bgm_SYS.srt",9,"st_j_nanika");
    conv1("bgm_SYS.srt",10,"st_j_termin");
    conv1("bgm_SYS.srt",11,"st_j_mumu");
    conv1("bgm_SYS.srt",12,"st_j_gameover");
    conv1("bgm_SYS.srt",13,"st_j_tekish");
    conv1("bgm_SYS.srt",14,"st_s_god");

    conv1("bgm_SYS.srt",15,"st_s_gaiko01");
    conv1("bgm_SYS.srt",16,"st_s_barbaros");
    /* or
    conv1("bgm_SYS.srt",16,"st_s_remocon");*/

    conv1("bgm_SYS.srt",17,"st_s_gaiko02");
    conv1("bgm_SYS.srt",18,"st_s_gaiko03");
    conv1("bgm_SYS.srt",19,"st_s_gaiko04");
    conv1("bgm_SYS.srt",20,"st_s_gaiko05");
    conv1("bgm_SYS.srt",21,"st_s_gaiko06");
    conv1("bgm_SYS.srt",22,"st_j_rankup");
    conv1("bgm_SYS.srt",23,"st_s_coinpod");
    conv1("bgm_SYS.srt",24,"st_j_tanken");
    conv1("bgm_SYS.srt",25,"st_j_kakushi");
    conv1("bgm_SYS.srt",26,"st_s_god_f");
    conv1("bgm_SYS.srt",27,"st_s_god_m");
}
