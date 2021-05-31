#pragma once
#include <stdio.h>
#include <string.h>

enum VIDEO_DEVICE_GROUP_TYPE {
    VIDEO_DEVICE_GROUP_STANDARD = 0, // 8029, 8036, 8037, 8052, 8053, 8059
    VIDEO_DEVICE_GROUP_MULTIBASELINE, // 8038
    VIDEO_DEVICE_GROUP_KOLOR, // 8040, 8054
    VIDEO_DEVICE_GROUP_KOLOR_TRACK, // 8060
    VIDEO_DEVICE_GROUP_GRAP, // Grap
    VIDEO_DEVICE_GROUP_ANONYMOUS
};

enum VIDEO_DEVICE_GROUP_DEVICE_TYPE {
    VIDEO_DEVICE_GROUP_DEVICE_MASTER = 0,
    VIDEO_DEVICE_GROUP_DEVICE_COLOR,
    VIDEO_DEVICE_GROUP_DEVICE_DEPTH,
    VIDEO_DEVICE_GROUP_DEVICE_DEPTH_30mm,
    VIDEO_DEVICE_GROUP_DEVICE_DEPTH_60mm,
    VIDEO_DEVICE_GROUP_DEVICE_MASTER_150mm,
    VIDEO_DEVICE_GROUP_DEVICE_DEPTH_150mm,
    VIDEO_DEVICE_GROUP_DEVICE_COLOR_WITH_DEPTH,
    VIDEO_DEVICE_GROUP_DEVICE_KOLOR,
    VIDEO_DEVICE_GROUP_DEVICE_TRACK,
    VIDEO_DEVICE_GROUP_DEVICE_COLOR_SLAVE,
    VIDEO_DEVICE_GROUP_DEVICE_KOLOR_SLAVE
    //+[Thermal device]
    ,VIDEO_DEVICE_GROUP_DEVICE_THERMAL
    //-[Thermal device]
};

class CVideoDevice;
class CVideoDeviceGroup {
public:
    virtual ~CVideoDeviceGroup() = default;
    virtual bool IsGroup(CVideoDevice *pDevice);
    virtual int AddDevice(CVideoDevice *pDevice);
    virtual CVideoDevice *GetDevice(VIDEO_DEVICE_GROUP_DEVICE_TYPE devType) = 0;
    virtual bool IsValid() = 0;

    VIDEO_DEVICE_GROUP_TYPE GetGroupType(){ return m_GroupType; }

protected:
    CVideoDeviceGroup():
    m_GroupType(VIDEO_DEVICE_GROUP_ANONYMOUS)
    {
        memset(m_pGrouID, 0, sizeof(m_pGrouID));
    }

    VIDEO_DEVICE_GROUP_TYPE m_GroupType;
    char m_pGrouID[64];
};

class CVideoDeviceGroup_Standard : public CVideoDeviceGroup {
public:
    virtual ~CVideoDeviceGroup_Standard() = default;
    virtual int AddDevice(CVideoDevice *pDevice);
    virtual CVideoDevice *GetDevice(VIDEO_DEVICE_GROUP_DEVICE_TYPE devType);
    virtual bool IsValid();

    friend class CVideoDeviceGroupFactory;
private:
    CVideoDeviceGroup_Standard():
    m_pColorDevice(nullptr),
    m_pDepthDevice(nullptr)
    {
        m_GroupType = VIDEO_DEVICE_GROUP_STANDARD;
    }

private:
    CVideoDevice *m_pColorDevice;
    CVideoDevice *m_pDepthDevice;
};

class CVideoDeviceGroup_MultiBaseline : public CVideoDeviceGroup {
public:
    virtual ~CVideoDeviceGroup_MultiBaseline() = default;

    virtual int AddDevice(CVideoDevice *pDevice);
    virtual int AddDevice(CVideoDeviceGroup_MultiBaseline *pGroup);
    virtual CVideoDevice *GetDevice(VIDEO_DEVICE_GROUP_DEVICE_TYPE devType);
    virtual bool IsValid();

    friend class CVideoDeviceGroupFactory;
private:
    CVideoDeviceGroup_MultiBaseline():
    m_pColorWithDepthDevice(nullptr),
    m_pDepthDevice_60mm(nullptr),
    m_pMasterDevice_150mm(nullptr),
    m_pDepthDevice_150mm(nullptr)
    {
        m_GroupType = VIDEO_DEVICE_GROUP_MULTIBASELINE;
    }

private:
    CVideoDevice *m_pColorWithDepthDevice;
    CVideoDevice *m_pDepthDevice_60mm;
    CVideoDevice *m_pMasterDevice_150mm;
    CVideoDevice *m_pDepthDevice_150mm;
};

class CVideoDeviceGroup_Kolor : public CVideoDeviceGroup {
public:
    virtual ~CVideoDeviceGroup_Kolor() = default;

    virtual bool IsGroup(CVideoDevice *pDevice);
    virtual int AddDevice(CVideoDevice *pDevice);
    virtual CVideoDevice *GetDevice(VIDEO_DEVICE_GROUP_DEVICE_TYPE devType);
    virtual bool IsValid();

    friend class CVideoDeviceGroupFactory;
private:
    CVideoDeviceGroup_Kolor():
    m_pColorWithDepthDevice(nullptr),
    m_pDepthDevice(nullptr),
    m_pKolorDevice(nullptr)
    {
        m_GroupType = VIDEO_DEVICE_GROUP_KOLOR;
    }

private:
    CVideoDevice *m_pColorWithDepthDevice;
    CVideoDevice *m_pDepthDevice;
    CVideoDevice *m_pKolorDevice;
};

class CVideoDeviceGroup_Kolor_Track : public CVideoDeviceGroup {
public:
    virtual ~CVideoDeviceGroup_Kolor_Track() = default;

    virtual bool IsGroup(CVideoDevice *pDevice);
    virtual int AddDevice(CVideoDevice *pDevice);
    virtual CVideoDevice *GetDevice(VIDEO_DEVICE_GROUP_DEVICE_TYPE devType);
    virtual bool IsValid();

    friend class CVideoDeviceGroupFactory;
private:
    CVideoDeviceGroup_Kolor_Track():
    m_pColorDevice(nullptr),
    m_pDepthDevice(nullptr),
    m_pKolorDevice(nullptr),
    m_pTrackDevice(nullptr)
    {
        m_GroupType = VIDEO_DEVICE_GROUP_KOLOR_TRACK;
    }

private:
    CVideoDevice *m_pColorDevice;
    CVideoDevice *m_pDepthDevice;
    CVideoDevice *m_pKolorDevice;
    CVideoDevice *m_pTrackDevice;
};

class CVideoDeviceGroup_Grap : public CVideoDeviceGroup
{
public:
    virtual ~CVideoDeviceGroup_Grap() = default;

    virtual bool IsGroup(CVideoDevice *pDevice);
    virtual int AddDevice(CVideoDevice *pDevice);
    virtual int AddDevice(CVideoDeviceGroup_Grap *pGroup);
    virtual CVideoDevice *GetDevice(VIDEO_DEVICE_GROUP_DEVICE_TYPE devType);
    virtual bool IsValid();

    friend class CVideoDeviceGroupFactory;

private:
    CVideoDeviceGroup_Grap() : m_pColorDevice_Master(nullptr),
                               m_pKolorDevice_Master(nullptr),
                               m_pColorDevice_Slave(nullptr),
                               m_pKolorDevice_Slave(nullptr)
                               //+[Thermal device]
                               ,m_pThermalDevice(nullptr)
                               //-[Thermal device]
    {
        m_GroupType = VIDEO_DEVICE_GROUP_GRAP;
    }

private:
    CVideoDevice *m_pColorDevice_Master;
    CVideoDevice *m_pKolorDevice_Master;
    CVideoDevice *m_pColorDevice_Slave;
    CVideoDevice *m_pKolorDevice_Slave;
    //+[Thermal device]
    CVideoDevice *m_pThermalDevice;
    //-[Thermal device]
};

class CVideoDeviceGroup_Anonymous : public CVideoDeviceGroup{
public:
    virtual ~CVideoDeviceGroup_Anonymous() = default;
    virtual int AddDevice(CVideoDevice *pDevice) {
        m_pDevice = pDevice;
    }
    virtual CVideoDevice *GetDevice(VIDEO_DEVICE_GROUP_DEVICE_TYPE devType) {
        return m_pDevice;
    }
    virtual bool IsValid() { return m_pDevice != nullptr; }

    friend class CVideoDeviceGroupFactory;
private:
    CVideoDeviceGroup_Anonymous()
    {
        m_pDevice = nullptr;
        m_GroupType = VIDEO_DEVICE_GROUP_ANONYMOUS;
    }

private:
    CVideoDevice *m_pDevice;
};

class CVideoDeviceGroupFactory {
public:
    static CVideoDeviceGroup *GenerateVideoGroupDevice(CVideoDevice *pDevice);
private:
    CVideoDeviceGroupFactory() = default;
    ~CVideoDeviceGroupFactory() = default;
};
