spi_write_command(spi, 0xE0); //P-Gamma
spi_write_data(spi, 0x00);
spi_write_data(spi, 0x10);
spi_write_data(spi, 0x14);
spi_write_data(spi, 0x01);
spi_write_data(spi, 0x0E);
spi_write_data(spi, 0x04);
spi_write_data(spi, 0x33);
spi_write_data(spi, 0x56);
spi_write_data(spi, 0x48);
spi_write_data(spi, 0x03);
spi_write_data(spi, 0x0C);
spi_write_data(spi, 0x0B);
spi_write_data(spi, 0x2B);
spi_write_data(spi, 0x34);
spi_write_data(spi, 0x0F);

spi_write_command(spi, 0xE1); //N-Gamma
spi_write_data(spi, 0x00);
spi_write_data(spi, 0x12);
spi_write_data(spi, 0x18);
spi_write_data(spi, 0x05);
spi_write_data(spi, 0x12);
spi_write_data(spi, 0x06);
spi_write_data(spi, 0x40);
spi_write_data(spi, 0x34);
spi_write_data(spi, 0x57);
spi_write_data(spi, 0x06);
spi_write_data(spi, 0x10);
spi_write_data(spi, 0x0C);
spi_write_data(spi, 0x3B);
spi_write_data(spi, 0x3F);
spi_write_data(spi, 0x0F);

spi_write_command(spi, 0XC0); //Power Control 1
spi_write_data(spi, 0x0F); //Vreg1out
spi_write_data(spi, 0x0C); //Verg2out

spi_write_command(spi, 0xC1); //Power Control 2
spi_write_data(spi, 0x41); //VGH,VGL

spi_write_command(spi, 0xC5); //Power Control 3
spi_write_data(spi, 0x00);
spi_write_data(spi, 0x25);    //Vcom
spi_write_data(spi, 0x80);

spi_write_command(spi, 0x36); //Memory Access
spi_write_data(spi, 0x48);

spi_write_command(spi, 0x3A); //Interface Pixel Format
spi_write_data(spi, 0x66); //18bit

spi_write_command(spi, 0XB0); //Interface Mode Control
spi_write_data(spi, 0x00); //18bit

spi_write_command(spi, 0xB1); //Frame rate
spi_write_data(spi, 0xA0); //60Hz

spi_write_command(spi, 0xB4); //Display Inversion Control
spi_write_data(spi, 0x02); //2-dot

spi_write_command(spi, 0XB6); //RGB/MCU Interface Control
spi_write_data(spi, 0x32); //MCU:02; RGB:32/22
spi_write_data(spi, 0x02); //Source,Gate scan dieection

spi_write_command(spi, 0XE9); //Set Image Function
spi_write_data(spi, 0x00); //disable 24 bit data input

spi_write_command(spi, 0xF7); //   A d j u s t   C o n t r o l
spi_write_data(spi, 0xA9);
spi_write_data(spi, 0x51);
spi_write_data(spi, 0x2C);
spi_write_data(spi, 0x82); //   D 7   s t r e a m ,   l o o s e

spi_write_command(spi, 0x21); //Normal Black

spi_write_command(spi, 0x11); //Sleep out
rtos_time_delay_ms(120);
spi_write_command(spi, 0x29);//Display on