/* Pan Tilt Zoom camera instance
 *
 * Copyright 2020 Grant Likely <grant.likely@secretlab.ca>
 *
 * SPDX-License-Identifier: GPLv2
 */
#pragma once

#include <QObject>
#include <QTimer>
#include <QSerialPort>
#include <QUdpSocket>
#include "ptz-device.hpp"

class visca_encoding {
public:
	const char *name;
	int offset;
	visca_encoding(const char *name, int offset) : name(name), offset(offset) { }
	virtual void encode(QByteArray &data, int val) = 0;
	virtual int decode(QByteArray &data) = 0;
};
class visca_u4 : public visca_encoding {
public:
	visca_u4(const char *name, int offset) : visca_encoding(name, offset) { }
	void encode(QByteArray &data, int val) {
		if (data.size() < offset + 1)
			return;
		data[offset] = data[offset] & 0xf0 | val & 0x0f;
	}
	int decode(QByteArray &data) {
		return (data.size() < offset + 1) ? 0 : data[offset] & 0xf;
	}
};
class visca_flag : public visca_encoding {
public:
	visca_flag(const char *name, int offset) : visca_encoding(name, offset) { }
	void encode(QByteArray &data, int val) {
		if (data.size() < offset + 1)
			return;
		data[offset] = val ? 0x2 : 0x3;
	}
	int decode(QByteArray &data) {
		return (data.size() < offset + 1) ? 0 : 0x02 == data[offset];
	}
};
class visca_u7 : public visca_encoding {
public:
	visca_u7(const char *name, int offset) : visca_encoding(name, offset) { }
	void encode(QByteArray &data, int val) {
		if (data.size() < offset + 1)
			return;
		data[offset] = val & 0x7f;
	}
	int decode(QByteArray &data) {
		return (data.size() < offset + 1) ? 0 : data[offset] & 0x7f;
	}
};
class visca_s7 : public visca_encoding {
public:
	visca_s7(const char *name, int offset) : visca_encoding(name, offset) { }
	virtual void encode(QByteArray &data, int val) {
		if (data.size() < offset + 3)
			return;
		data[offset] = abs(val) & 0x7f;
		data[offset+2] = 3;
		if (val)
			data[offset+2] = val < 0 ? 1 : 2;

	}
	virtual int decode(QByteArray &data) {
		return (data.size() < offset + 3) ? 0 : data[offset] & 0x7f;
	}
};
class visca_u16 : public visca_encoding {
public:
	visca_u16(const char *name, int offset) : visca_encoding(name, offset) { }
	void encode(QByteArray &data, int val) {
		if (data.size() < offset + 4)
			return;
		data[offset] = (val >> 12) & 0x0f;
		data[offset+1] = (val >> 8) & 0x0f;
		data[offset+2] = (val >> 4) & 0x0f;
		data[offset+3] = val & 0x0f;

	}
	int decode(QByteArray &data) {
		if (data.size() < offset + 4)
			return 0;
		return (data[offset] & 0xf) << 12 |
		       (data[offset+1] & 0xf) << 8 |
		       (data[offset+2] & 0xf) << 4 |
		       (data[offset+3] & 0xf);
	}
};

class ViscaCmd {
public:
	QByteArray cmd;
	const QList<visca_encoding*> args;
	const QList<visca_encoding*> results;
	ViscaCmd(const char *cmd_hex) : cmd(QByteArray::fromHex(cmd_hex)) { }
	ViscaCmd(const char *cmd_hex, QList<visca_encoding*> args) :
		cmd(QByteArray::fromHex(cmd_hex)), args(args) { }
	ViscaCmd(const char *cmd_hex, QList<visca_encoding*> args, QList<visca_encoding*> rslts) :
		cmd(QByteArray::fromHex(cmd_hex)), args(args), results(rslts) { }
	void encode(int address) {
		cmd[0] = (char)(0x80 | address & 0x7);
	}
	void encode(int address, QList<int> arglist) {
		encode(address);
		for (int i = 0; i < arglist.size(), i < args.size(); i++)
			args[i]->encode(cmd, arglist[i]);
	}
	void decode(QObject *target, QByteArray msg) {
		for (int i = 0; i < results.size(); i++)
			target->setProperty(results[i]->name, results[i]->decode(msg));
	}
};
class ViscaInq : public ViscaCmd {
public:
	ViscaInq(const char *cmd_hex) : ViscaCmd(cmd_hex) { }
	ViscaInq(const char *cmd_hex, QList<visca_encoding*> rslts) : ViscaCmd(cmd_hex, {}, rslts) {}
};


/*
 * ViscaUART implementing UART protocol
 */
class ViscaUART : public QObject {
	Q_OBJECT

private:
	/* Global lookup table of UART instances, used to eliminate duplicates */
	static std::map<QString, ViscaUART*> interfaces;

	QString port_name;
	QSerialPort uart;
	QByteArray rxbuffer;
	int camera_count;

signals:
	void receive_ack(const QByteArray &packet);
	void receive_complete(const QByteArray &packet);
	void receive_error(const QByteArray &packet);
	void reset();

public:
	ViscaUART(QString &port_name);
	void open();
	void close();
	virtual void send(const QByteArray &packet);
	virtual void receive(const QByteArray &packet);
	QString portName() { return port_name; }

	static ViscaUART *get_interface(QString port_name);

public slots:
	virtual void poll();
};

class PTZVisca : public PTZDevice {
	Q_OBJECT

private:
	ViscaUART *iface;
	unsigned int address;
	QList<ViscaCmd> pending_cmds;
	bool active_cmd;
	QTimer timeout_timer;

	void reset();
	void attach_interface(ViscaUART *iface);
	void send_pending();
	void send(const ViscaCmd &cmd);
	void send(const ViscaCmd &cmd, QList<int> args);
	void timeout();

private slots:
	void receive_ack(const QByteArray &msg);
	void receive_complete(const QByteArray &msg);

public:
	PTZVisca(QString uart_name, unsigned int address);
	PTZVisca(OBSData config);
	~PTZVisca();

	void set_config(OBSData ptz_data);
	OBSData get_config();

	void cmd_get_camera_info();
	void pantilt(double pan, double tilt);
	void pantilt_rel(int pan, int tilt);
	void pantilt_stop();
	void pantilt_home();
	void zoom_stop();
	void zoom_tele();
	void zoom_wide();
	void memory_reset(int i);
	void memory_set(int i);
	void memory_recall(int i);
};

class ViscaUDPSocket : public ViscaUART {
	Q_OBJECT

private:
	int visca_port;
	QUdpSocket visca_socket;
	int discovery_port;
	QUdpSocket discovery_socket;
	QHostAddress camera_address;

public:
	ViscaUDPSocket(int port = 52381, int discovery_port = 51380);
	void receive_datagram(QNetworkDatagram &datagram);
	virtual void send(const QByteArray &packet);

public slots:
	virtual void poll();
	virtual void poll_discovery();
};


