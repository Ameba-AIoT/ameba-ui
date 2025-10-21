spi_write_command(0xFF);
spi_write_data(0x77);
spi_write_data(0x01);
spi_write_data(0x00);
spi_write_data(0x00);
spi_write_data(0x13);

spi_write_command(0xEF);
spi_write_data(0x08);

spi_write_command(0x3A);
spi_write_data(0x70);

spi_write_command(0xFF);
spi_write_data(0x77);
spi_write_data(0x01);
spi_write_data(0x00);
spi_write_data(0x00);
spi_write_data(0x10);

spi_write_command(0xC0); // Display Line Setting
spi_write_data(0x3B);
spi_write_data(0x00);

spi_write_command(0xC1); // Porch Control
spi_write_data(0x09);
spi_write_data(0x05);

spi_write_command(0xC2); // Inversion set
spi_write_data(0x07);    // 31 2-DOT 37-Column
spi_write_data(0x02);

spi_write_command(0xC6); // SET RGB MODE
spi_write_data(0x21);    // 00-DE MODE ,80-HV MODE	 PCLK N

spi_write_command(0xCC); // SET RGB MODE
spi_write_data(0x30);    // 00-DE MODE ,80-HV MODE	 PCLK N

spi_write_command(0xB0);
spi_write_data(0xC0);    // Positive Voltage Gamma Control
spi_write_data(0x54);
spi_write_data(0x5C);
spi_write_data(0x0D);
spi_write_data(0x51);
spi_write_data(0x06);
spi_write_data(0x09);
spi_write_data(0x08);
spi_write_data(0x07);
spi_write_data(0x24);
spi_write_data(0x03);
spi_write_data(0x11);
spi_write_data(0x0F);
spi_write_data(0xAC);
spi_write_data(0xB5);
spi_write_data(0x7F);

spi_write_command(0xB1);
spi_write_data(0xC0);     // Negative Voltage Gamma Control
spi_write_data(0x54);
spi_write_data(0x5C);
spi_write_data(0x0E);
spi_write_data(0x11);
spi_write_data(0x07);
spi_write_data(0x0A);
spi_write_data(0x09);
spi_write_data(0x08);
spi_write_data(0x24);
spi_write_data(0x04);
spi_write_data(0x51);
spi_write_data(0x10);
spi_write_data(0xAD);
spi_write_data(0x75);
spi_write_data(0x7F);

//*******power set********//
//PAGE2
spi_write_command(0xFF);
spi_write_data(0x77);
spi_write_data(0x01);
spi_write_data(0x00);
spi_write_data(0x00);
spi_write_data(0x11);

spi_write_command(0xB0);   // Vop Amplitude setting
spi_write_data(0x7D);      // Vop=4.7375v

spi_write_command(0xB1);   // VCOM amplitude setting
spi_write_data(0x33);      // VCOM=32

spi_write_command(0xB2);   // VGH Voltage setting
spi_write_data(0x87);      // VGH=15v

spi_write_command(0xB3);   // TEST Command Setting
spi_write_data(0x80);

spi_write_command(0xB5);   // VGL Voltage setting
spi_write_data(0x45);      // VGL=-10.17v

spi_write_command(0xB7);   // Power Control 1
spi_write_data(0x87);

spi_write_command(0xB8);   // Power Control 2
spi_write_data(0x33);      // AVDD=6.6 & AVCL=-4.6

spi_write_command(0xB9);   // Power Control 2
spi_write_data(0x10);      // AVDD=6.6 & AVCL=-4.6

spi_write_command(0xBB);   // Power Control 2
spi_write_data(0x03);      // AVDD=6.6 & AVCL=-4.6

spi_write_command(0xC0);   // Source pre_drive timing set1
spi_write_data(0x03);

spi_write_command(0xC1);   // Source pre_drive timing set1
spi_write_data(0x78);

spi_write_command(0xC2);   // Source EQ2 Setting
spi_write_data(0x78);

spi_write_command(0xD0);   // Source EQ2 Setting
spi_write_data(0x88);

//*********GIP SET************//
spi_write_command(0xE0);
spi_write_data(0x00);
spi_write_data(0x18);
spi_write_data(0x00);
spi_write_data(0x00);
spi_write_data(0x00);
spi_write_data(0x20);

spi_write_command(0xE1);
spi_write_data(0x05);
spi_write_data(0xA0);
spi_write_data(0x00);
spi_write_data(0xA0);
spi_write_data(0x04);
spi_write_data(0x0A);
spi_write_data(0x00);
spi_write_data(0xA0);
spi_write_data(0x00);
spi_write_data(0x44);
spi_write_data(0x44);

spi_write_command(0xE2);
spi_write_data(0x11);
spi_write_data(0x11);
spi_write_data(0x44);
spi_write_data(0x44);
spi_write_data(0xEA);
spi_write_data(0xA0);
spi_write_data(0x00);
spi_write_data(0x00);
spi_write_data(0xE9);
spi_write_data(0xA0);
spi_write_data(0x00);
spi_write_data(0x00);

spi_write_command(0xE3);
spi_write_data(0x00);
spi_write_data(0x00);
spi_write_data(0x11);
spi_write_data(0x11);

spi_write_command(0xE4);
spi_write_data(0x44);
spi_write_data(0x44);

spi_write_command(0xE5);
spi_write_data(0x06);
spi_write_data(0xE5);
spi_write_data(0xD8);
spi_write_data(0xA0);
spi_write_data(0x08);
spi_write_data(0xE7);
spi_write_data(0xD8);
spi_write_data(0xA0);
spi_write_data(0x0A);
spi_write_data(0xE9);
spi_write_data(0xD8);
spi_write_data(0xA0);
spi_write_data(0x0C);
spi_write_data(0xEB);
spi_write_data(0xD8);
spi_write_data(0xA0);

spi_write_command(0xE6);
spi_write_data(0x00);
spi_write_data(0x00);
spi_write_data(0x11);
spi_write_data(0x11);

spi_write_command(0xE7);
spi_write_data(0x44);
spi_write_data(0x44);

spi_write_command(0xE8);
spi_write_data(0x05);
spi_write_data(0xE4);
spi_write_data(0xD8);
spi_write_data(0xA0);
spi_write_data(0x07);
spi_write_data(0xE6);
spi_write_data(0xD8);
spi_write_data(0xA0);
spi_write_data(0x09);
spi_write_data(0xE8);
spi_write_data(0xD8);
spi_write_data(0xA0);
spi_write_data(0x0B);
spi_write_data(0xEA);
spi_write_data(0xD8);
spi_write_data(0xA0);

spi_write_command(0xEB);
spi_write_data(0x02);
spi_write_data(0x00);
spi_write_data(0xE4);
spi_write_data(0xE4);
spi_write_data(0x88);
spi_write_data(0x00);
spi_write_data(0x10);

spi_write_command(0xEC);
spi_write_data(0x3D);
spi_write_data(0x02);
spi_write_data(0x00);

spi_write_command(0xED);
spi_write_data(0x20);
spi_write_data(0x76);
spi_write_data(0x54);
spi_write_data(0x98);
spi_write_data(0xBA);
spi_write_data(0xFF);
spi_write_data(0xFF);
spi_write_data(0xFF);
spi_write_data(0xFF);
spi_write_data(0xFF);
spi_write_data(0xFF);
spi_write_data(0xAB);
spi_write_data(0x89);
spi_write_data(0x45);
spi_write_data(0x67);
spi_write_data(0x02);

//-----------VAP & VAN---------------
spi_write_command(0xEF);
spi_write_data(0x08);
spi_write_data(0x08);
spi_write_data(0x08);
spi_write_data(0x45);
spi_write_data(0x3F);
spi_write_data(0x54);


spi_write_command(0xFF);
spi_write_data(0x77);
spi_write_data(0x01);
spi_write_data(0x00);
spi_write_data(0x00);
spi_write_data(0x13);

spi_write_command(0xE8);
spi_write_data(0x00);
spi_write_data(0x0E);

spi_write_command(0xE8);
spi_write_data(0x00);
spi_write_data(0x0C);

spi_write_command(0xE8);
spi_write_data(0x00);
spi_write_data(0x00);

spi_write_command(0xFF);    // page
spi_write_data(0x77);
spi_write_data(0x01);
spi_write_data(0x00);
spi_write_data(0x00);
spi_write_data(0x00);

spi_write_command(0x36);
spi_write_data(0x00);       // 10-180

spi_write_command(0x11);

DelayMs(90);
spi_write_command(0x29);
spi_write_command(0x21);

//Display on
DelayMs(25);
