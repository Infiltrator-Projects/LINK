// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/j1979da.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr,"failed %s:%d: %s\n",__FILE__,__LINE__,#c); return 1; } } while (0)

int main(void)
{
    char command[16];
    const LinkJ1979Mode05TidDefinition *m05;
    const LinkJ1979Mode06MonitorDefinition *mid;
    const LinkJ1979UnitScaling *scale;
    LinkJ1979Mode05Result r05;
    LinkJ1979Mode06ResultList r06;
    const uint8_t mode05_response[]={0x45U,0x05U,0x01U,0x64U,0x10U,0xC8U};
    const uint8_t mode06_response[]={
        0x46U,
        0x21U,0x01U,0x0AU,0x10U,0x00U,0x08U,0x00U,0x20U,0x00U,
        0xA2U,0x0BU,0x24U,0x00U,0x64U,0x00U,0x00U,0x01U,0x00U
    };
    const uint8_t malformed06[]={0x46U,0x21U,0x01U};

    CHECK(strcmp(link_j1979_revision(),"J1979_202505")==0);
    CHECK(strcmp(link_j1979da_revision(),"J1979DA_202607")==0);
    CHECK(strcmp(link_j1978_1_revision(),"J1978-1_202604")==0);
    CHECK(strcmp(link_j1979_2_revision(),"J1979-2_202604")==0);

    m05=link_j1979_mode05_tid_definition(0x01U);
    CHECK(m05!=NULL && fabs(m05->scale-0.005)<0.0000001);
    CHECK(link_j1979_mode05_tid_classification(0x21U)==LINK_J1979_IDENTIFIER_MANUFACTURER_DEFINED);
    CHECK(link_j1979_build_mode05_request(0x01U,0x01U,command,sizeof(command))==LINK_OBD2_RESULT_OK);
    CHECK(strcmp(command,"050101")==0);
    CHECK(link_j1979_decode_mode05_response(mode05_response,sizeof(mode05_response),&r05)==LINK_OBD2_RESULT_OK);
    CHECK(r05.scaling_known && r05.limits_available);
    CHECK(fabs(r05.value-0.4)<0.0000001);

    mid=link_j1979_mode06_monitor_definition(0x01U);
    CHECK(mid!=NULL && strstr(mid->name,"B1S1")!=NULL);
    mid=link_j1979_mode06_monitor_definition(0xB2U);
    CHECK(mid!=NULL && strstr(mid->name,"filter")!=NULL);
    CHECK(link_j1979_mode06_mid_classification(0xE1U)==LINK_J1979_IDENTIFIER_MANUFACTURER_DEFINED);
    CHECK(link_j1979_mode06_mid_classification(0x11U)==LINK_J1979_IDENTIFIER_STANDARD);
    CHECK(link_j1979_mode06_monitor_definition(0x11U)==NULL);
    CHECK(link_j1979_mode06_mid_classification(0x51U)==LINK_J1979_IDENTIFIER_STANDARD);
    CHECK(link_j1979_mode06_mid_classification(0xC1U)==LINK_J1979_IDENTIFIER_UNVERIFIED);
    CHECK(link_j1979_mode06_tid_classification(0x20U)==LINK_J1979_IDENTIFIER_UNVERIFIED);
    CHECK(link_j1979_mode06_tid_classification(0xFFU)==LINK_J1979_IDENTIFIER_RESERVED);

    scale=link_j1979_mode06_uasid_definition(0x0AU);
    CHECK(scale!=NULL && fabs(scale->scale-0.122)<0.0000001);
    CHECK(link_j1979_mode06_uasid_classification(0x45U)==LINK_J1979_IDENTIFIER_STANDARD);
    CHECK(link_j1979_mode06_uasid_definition(0x45U)==NULL);
    CHECK(link_j1979_mode06_uasid_classification(0x42U)==LINK_J1979_IDENTIFIER_UNVERIFIED);
    CHECK(link_j1979_mode09_infotype_classification(0x12U)==LINK_J1979_IDENTIFIER_STANDARD);
    CHECK(link_j1979_mode09_infotype_classification(0x79U)==LINK_J1979_IDENTIFIER_STANDARD);
    CHECK(link_j1979_mode09_infotype_classification(0x30U)==LINK_J1979_IDENTIFIER_UNVERIFIED);
    scale=link_j1979_mode06_uasid_definition(0x8CU);
    CHECK(scale!=NULL && scale->signed_value);
    CHECK(fabs(link_j1979_mode06_apply_scaling(scale,UINT16_C(0xFFFF))+0.01)<0.0000001);

    CHECK(link_j1979_decode_mode06_response(mode06_response,sizeof(mode06_response),&r06)==LINK_OBD2_RESULT_OK);
    CHECK(r06.count==2U);
    CHECK(r06.entries[0].mid==0x21U && r06.entries[0].tid==0x01U);
    CHECK(r06.entries[0].scaling_known && r06.entries[0].pass_known && r06.entries[0].passed);
    CHECK(fabs(r06.entries[0].value-499.712)<0.001);
    CHECK(r06.entries[1].monitor!=NULL && r06.entries[1].test!=NULL &&
          r06.entries[1].scaling!=NULL && r06.entries[1].value==100.0);
    CHECK(link_j1979_decode_mode06_response(malformed06,sizeof(malformed06),&r06)==LINK_OBD2_RESULT_MALFORMED_RESPONSE);

    puts("J1979/J1979-DA semantic tests passed");
    return 0;
}
