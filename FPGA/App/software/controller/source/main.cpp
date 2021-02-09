#include <stdio.h>
//#include <math.h>
//#include <sys/alt_irq.h>
#include <sys/alt_stdio.h>
#include <microshell/core/microshell.h>
#include <microshell/util/mscmd.h>
#include <microshell/util/ntlibc.h>
//#include <altera_msgdma.h>
//#include <altera_avalon_pio_regs.h>
#include <peripheral/motor_controller.hpp>
#include <peripheral/vector_controller.hpp>
#include <driver/adc2.hpp>
#include <driver/imu.hpp>
#include <driver/led.hpp>
#include <driver/load_switch.hpp>
#include <driver/critical_section.hpp>
#include "shared_memory.hpp"
#include "centralized_monitor.hpp"
#include "wheel_controller.hpp"
#include "dribble_controller.hpp"
#include "stream_transmitter.hpp"

// Memo : IRQ and priorities
// msgdma_0    IRQ0, RIL=1, RRS=1
// timer_0     IRQ1, RIL=3, RRS=3
// pio_0       IRQ2, RIL=2, RRS=2
// pio_1       IRQ3, RIL=3, RRS=3
// jtag_uart_0 IRQ4, RIL=1, RRS=1
// i2cm_0      IRQ5, RIL=1, RRS=1
// spim_0      IRQ6, RIL=1, RRS=1
// vcm_0       IRQ7, RIL=3, RRS=3
// mc_5        IRQ8, RIL=3, RRS=3

//bool g_speed_control = false;
//int g_speed = 0;
//int g_kp = 0;
//int g_ki = 0;
//float g_current = 0;
//int g_error = 0;

/*void Pulse1kHzHandler(void *context) {
 IOWR_ALTERA_AVALON_PIO_EDGE_CAP(PIO_0_BASE, 0);

 if (g_speed_control == true){
 int speed = VectorController::GetEncoderValue(1);
 int error = g_speed - speed;
 float value_i = (float)g_ki * error;
 float value_p = (float)g_kp * (error - g_error);
 g_error = error;

 float current = g_current;
 current += (value_p + value_i) * 0.0000152587890625f;
 current = fmaxf(-0.5f, fminf(current, 0.5f));
 g_current = current;
 int current_i = current * 1977;
 VectorController::SetCurrentReferenceD(1, 0);
 VectorController::SetCurrentReferenceQ(1, current_i);
 }
 else{
 g_error = 0;
 g_current = 0;
 }

 static int cnt = 0;
 ++cnt;
 if (cnt == 50){
 Led::SetMotor5Enabled(true);
 }
 else if (100 <= cnt){
 cnt = 0;
 Led::SetMotor5Enabled(false);
 }
 static struct {
 uint16_t frame_number = 0;
 uint16_t pos_theta;
 int32_t error;
 float current;
 int16_t adc1_u_data_1;
 int16_t adc1_v_data_1;
 int16_t current_d;
 int16_t current_q;
 int16_t speed;
 } payload;

 uint32_t adc1_data = IORD_ALTERA_AVALON_PIO_DATA(PIO_3_BASE);
 uint32_t pos_data = IORD_ALTERA_AVALON_PIO_DATA(PIO_4_BASE);
 payload.frame_number++;
 payload.adc1_u_data_1 = adc1_data >> 16;
 payload.adc1_v_data_1 = adc1_data & 0xFFFF;
 payload.pos_theta = pos_data & 0xFFFF;
 payload.current_d = VectorController::GetCurrentMeasurementD(1);
 payload.current_q = VectorController::GetCurrentMeasurementQ(1);
 payload.speed = VectorController::GetEncoderValue(1);
 payload.error = g_error;
 payload.current = g_current;

 alt_u32 option = (0x72 << ALTERA_MSGDMA_DESCRIPTOR_CONTROL_TRANSMIT_CHANNEL_OFFSET) | ALTERA_MSGDMA_DESCRIPTOR_CONTROL_GENERATE_SOP_MASK | ALTERA_MSGDMA_DESCRIPTOR_CONTROL_GENERATE_EOP_MASK;
 alt_msgdma_standard_descriptor desc;
 alt_msgdma_dev *dev = alt_msgdma_open(MSGDMA_0_CSR_NAME);
 alt_msgdma_construct_standard_mm_to_st_descriptor(dev, &desc, reinterpret_cast<uint32_t*>(&payload), sizeof(payload), option);
 alt_msgdma_standard_descriptor_sync_transfer(dev, &desc);
 }*/

static MSCMD_USER_RESULT command_imu(MSOPT *msopt, MSCMD_USER_OBJECT usrobj) {
    ImuResult_t result;
    Imu::ReadData(&result);
    printf("Accel(%d, %d, %d) Gyro(%d, %d, %d)\n", result.AccelDataX, result.AccelDataY, result.AccelDataZ, result.GyroDataX, result.GyroDataY, result.GyroDataZ);
    return 0;
}

static MSCMD_USER_RESULT command_adc2(MSOPT *msopt, MSCMD_USER_OBJECT usrobj) {
    int p48v = Adc2::GetDc48v();
    int dribble = Adc2::GetDribbleCurrent();
    printf("P48V=%dmV Idribble=%dmA\n", p48v, dribble);
    return 0;
}

static MSCMD_USER_RESULT command_kp(MSOPT *msopt, MSCMD_USER_OBJECT usrobj) {
    int argc;
    msopt_get_argc(msopt, &argc);
    if (argc == 2) {
        char buf[MSCONF_MAX_INPUT_LENGTH];
        msopt_get_argv(msopt, 1, buf, sizeof(buf));
        int value = ntlibc_atoi(buf);
        float ki;
        WheelController::GetGains(nullptr, &ki);
        WheelController::SetGains((float)value, ki);
        printf("Kp = %d\n", value);
    }
    return 0;
}

static MSCMD_USER_RESULT command_ki(MSOPT *msopt, MSCMD_USER_OBJECT usrobj) {
    int argc;
    msopt_get_argc(msopt, &argc);
    if (argc == 2) {
        char buf[MSCONF_MAX_INPUT_LENGTH];
        msopt_get_argv(msopt, 1, buf, sizeof(buf));
        int value = ntlibc_atoi(buf);
        float kp;
        WheelController::GetGains(&kp, nullptr);
        WheelController::SetGains(kp, (float)value);
        printf("Ki = %d\n", value);
    }
    return 0;
}

/*static MSCMD_USER_RESULT command_speed(MSOPT *msopt, MSCMD_USER_OBJECT usrobj) {
 int argc;
 msopt_get_argc(msopt, &argc);
 if (argc == 1){
 printf("Speed => %d\n", g_speed);
 }
 else {
 char buf[MSCONF_MAX_INPUT_LENGTH];
 msopt_get_argv(msopt, 1, buf, sizeof(buf));
 g_speed = ntlibc_atoi(buf);
 g_speed_control = true;
 }
 return 0;
 }

 static MSCMD_USER_RESULT command_power(MSOPT *msopt, MSCMD_USER_OBJECT usrobj) {
 int argc;
 msopt_get_argc(msopt, &argc);
 if (argc == 1){
 printf("Power => %d %d\n", VectorController::GetCurrentMeasurementD(1), VectorController::GetCurrentMeasurementQ(1));
 }
 else {
 g_speed_control = false;
 g_speed = 0;
 g_current = 0;
 g_error = 0;
 char buf[MSCONF_MAX_INPUT_LENGTH];
 msopt_get_argv(msopt, 1, buf, sizeof(buf));
 int value = ntlibc_atoi(buf);
 //MotorController::ClearFault();
 VectorController::ClearFault();
 //MotorController::SetPower(value);
 if (argc == 3) {
 msopt_get_argv(msopt, 2, buf, sizeof(buf));
 int value2 = ntlibc_atoi(buf);
 VectorController::SetCurrentReferenceD(value, 0);
 VectorController::SetCurrentReferenceQ(value, value2);
 }
 else {
 VectorController::SetCurrentReferenceD(1, 0);
 VectorController::SetCurrentReferenceQ(1, value);
 }
 printf("Power <= %d %d\n", VectorController::GetCurrentReferenceD(1), VectorController::GetCurrentReferenceQ(1));
 }
 return 0;
 }*/

static MSCMD_USER_RESULT command_switch(MSOPT *msopt, MSCMD_USER_OBJECT usrobj) {
    int argc;
    msopt_get_argc(msopt, &argc);
    if (argc == 2) {
        char buf[MSCONF_MAX_INPUT_LENGTH];
        msopt_get_argv(msopt, 1, buf, sizeof(buf));
        bool enabled = buf[0] == '1';
        LoadSwitch::SetMotor1Enabled(enabled);
        LoadSwitch::SetMotor2Enabled(enabled);
        LoadSwitch::SetMotor3Enabled(enabled);
        LoadSwitch::SetMotor4Enabled(enabled);
        LoadSwitch::SetMotor5Enabled(enabled);
        printf("Switch<=%d\n", enabled);
    }
    return 0;
}

static MSCMD_USER_RESULT command_status(MSOPT *msopt, MSCMD_USER_OBJECT usrobj) {
    printf("PIO1=%08X\n", IORD_ALTERA_AVALON_PIO_DATA(PIO_1_BASE));
    printf("Mc5Status=%04X (%04X)\n", MotorController::GetStatus().Status, MotorController::GetInterruptFlag().Status);
    printf("VecStatus=%04X (%04X)\n", VectorController::GetStatus().Status, VectorController::GetInterruptFlag().Status);
    printf("Error=%08X, Fault=%08X\n", (int)CentralizedMonitor::GetErrorFlags(), (int)CentralizedMonitor::GetFaultFlags());
    return 0;
}

static MSCMD_USER_RESULT command_fault(MSOPT *msopt, MSCMD_USER_OBJECT usrobj) {
    /*g_speed_control = false;
     g_speed = 0;
     g_current = 0;
     g_error = 0;*/
    VectorController::SetFault();
    MotorController::SetFault();
    return 0;
}

static MSCMD_USER_RESULT command_clear(MSOPT *msopt, MSCMD_USER_OBJECT usrobj) {
    //CentralizedMonitor::ClearErrorFlags();
    SharedMemory::WriteErrorFlags(0xFFFFFFFFUL);
    return 0;
}

static inline void initialize_peripheral(void) {
    Imu::Initialize();
    Adc2::Initialize();
    SharedMemory::Initialize();
    StreamTransmitter::Initialize();
    CentralizedMonitor::Initialize();
    WheelController::Initialize();
    DribbleController::Initialize();

    /*VectorController::SetFault();
     VectorController::SetCurrentReferenceD(1, 0);
     VectorController::SetCurrentReferenceQ(1, 0);
     VectorController::SetGainP(3568);
     VectorController::SetGainI(525);
     MotorController::SetFault();
     LoadSwitch::SetAllOn();*/
}

static inline void start_peripheral(void) {
    Adc2::Start();
    CentralizedMonitor::Start();
}

int main(void) {
    // ペリフェラルとハードウェアの初期化を行う
    {
        CriticalSection cs;
        initialize_peripheral();
        start_peripheral();
    }

    // MicroShellを初期化する
    char buf[64];
    MICROSHELL ms;
    MSCMD mscmd;
    microshell_init(
        &ms,
        [](char c) {alt_putchar(c);},
        [](void) -> char {return alt_getchar();},
        nullptr);
    static constexpr MSCMD_COMMAND_TABLE command_table[] = {
        {"imu", command_imu},
        {"adc", command_adc2},
        {"kp", command_kp},
        {"ki", command_ki},
        //{"speed", command_speed},
        //{"power", command_power},
        {"switch", command_switch},
        {"status", command_status},
        {"clear", command_clear},
        {"f", command_fault}
    };
    mscmd_init(&mscmd, const_cast<MSCMD_COMMAND_TABLE*>(command_table), sizeof(command_table) / sizeof(command_table[0]), nullptr);

    // 起動メッセージを表示する
    alt_putstr("Hello from Nios II!\n");

    // コマンド入力を受け付ける
    while (true) {
        microshell_getline(&ms, buf, sizeof(buf));
        MSCMD_USER_RESULT result;
        mscmd_execute(&mscmd, buf, &result);
    }

    return 0;
}