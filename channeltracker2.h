/*
    This file is part of Kismet

    Kismet is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Kismet is distributed in the hope that it will be useful,
      but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Kismet; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef __CHANNELTRACKER_V2_H__
#define __CHANNELTRACKER_V2_H__

#include "config.h"

#include <string>
#include <map>

#include "globalregistry.h"
#include "kis_mutex.h"
#include "trackedelement.h"
#include "kis_net_microhttpd.h"
#include "devicetracker_component.h"
#include "packetchain.h"
#include "timetracker.h"

// Can appear in the list as either a numerical frequency or a named
// channel
class Channeltracker_V2_Channel : public tracker_component, public SharedGlobalData {
public:
    Channeltracker_V2_Channel() :
        tracker_component() {
        register_fields();
        reserve_fields(NULL);
    }

    Channeltracker_V2_Channel(int in_id) :
        tracker_component(in_id) { 
        register_fields();
        reserve_fields(NULL);

        // last_device_sec = 0;
    }

    Channeltracker_V2_Channel(int in_id, std::shared_ptr<tracker_element_map> e) : 
        tracker_component(in_id) {

        register_fields();
        reserve_fields(e);

        // last_device_sec = 0;
    }

    virtual uint32_t get_signature() const override {
        return adler32_checksum("Channeltracker_V2_Channel");
    }

    virtual std::unique_ptr<tracker_element> clone_type() override {
        using this_t = std::remove_pointer<decltype(this)>::type;
        auto dup = std::unique_ptr<this_t>(new this_t());
        return std::move(dup);
    }

    virtual std::unique_ptr<tracker_element> clone_type(int in_id) override {
        using this_t = std::remove_pointer<decltype(this)>::type;
        auto dup = std::unique_ptr<this_t>(new this_t(in_id));
        return std::move(dup);
    }

    __Proxy(channel, std::string, std::string, std::string, channel);
    __Proxy(frequency, double, double, double, frequency);

    typedef kis_tracked_rrd<> uint64_rrd;

    __ProxyTrackable(packets_rrd, uint64_rrd, packets_rrd);
    __ProxyTrackable(data_rrd, uint64_rrd, data_rrd);
    __ProxyTrackable(device_rrd,uint64_rrd, device_rrd);

    __ProxyTrackable(signal_data, kis_tracked_signal_data, signal_data);

    /*
    // C++-domain map of devices we've seen in the last second for computing if we
    // increase the RRD record
    map<mac_addr, bool> seen_device_map;
    time_t last_device_sec;
    */

protected:
    // Timer for updating the device list
    int timer_id;

    virtual void register_fields() override {
        tracker_component::register_fields();

        register_field("kismet.channelrec.channel", "logical channel", &channel);
        register_field("kismet.channelrec.frequency", "physical frequency", &frequency);
        register_field("kismet.channelrec.packets_rrd", "packet count RRD", &packets_rrd);
        register_field("kismet.channelrec.data_rrd", "byte count RRD", &data_rrd);
        register_field("kismet.channelrec.device_rrd", "active devices RRD", &device_rrd);
        register_field("kismet.channelrec.signal", "signal records", &signal_data);
    }

    virtual void reserve_fields(std::shared_ptr<tracker_element_map> e) override {
        tracker_component::reserve_fields(e);

        // Don't fast-forward the device RRD
        device_rrd->update_before_serialize(false);
    }

    // Channel, as string - Logical channels
    std::shared_ptr<tracker_element_string> channel;

    // Frequency, for collating
    std::shared_ptr<tracker_element_double> frequency;

    // Packets per second RRD
    std::shared_ptr<kis_tracked_rrd<> > packets_rrd;

    // Data in bytes per second RRD
    std::shared_ptr<kis_tracked_rrd<> > data_rrd;

    // Devices active per second RRD
    std::shared_ptr<kis_tracked_rrd<> > device_rrd;

    // Overall signal data.  This could in theory be populated by spectrum
    // analyzers in the future as well.
    std::shared_ptr<kis_tracked_signal_data> signal_data;

};

class Channeltracker_V2 : public tracker_component, 
    public kis_net_httpd_cppstream_handler, public lifetime_global, 
    public time_tracker_event {
public:
    static std::string global_name() { return "CHANNEL_TRACKER"; }

    static std::shared_ptr<Channeltracker_V2> create_channeltracker(global_registry *in_globalreg) {
        std::shared_ptr<Channeltracker_V2> mon(new Channeltracker_V2(in_globalreg));
        in_globalreg->register_lifetime_global(mon);
        in_globalreg->insert_global(global_name(), mon);
        return mon;
    }

private:
    Channeltracker_V2(global_registry *in_globalreg);

public:
    virtual ~Channeltracker_V2();

    // HTTP API
    virtual bool httpd_verify_path(const char *path, const char *method);

    virtual void httpd_create_stream_response(Kis_Net_Httpd *httpd,
            Kis_Net_Httpd_Connection *connection,
            const char *url, const char *method, const char *upload_data,
            size_t *upload_data_size, std::stringstream &stream);

    // Timetracker API
    virtual int timetracker_event(int event_id);

    // Update device counts
    void update_device_counts(std::map<double, unsigned int> in_counts);

    int device_decay;

protected:
    kis_recursive_timed_mutex lock;

    std::shared_ptr<device_tracker> devicetracker;
    std::shared_ptr<Timetracker> timetracker;

    // Packetchain callback
    static int PacketChainHandler(CHAINCALL_PARMS);

    // Tracker component
    virtual void register_fields();

    // Seen channels as string-named channels, so logical channel allocation
    // per phy
    std::shared_ptr<tracker_element_string_map> channel_map;

    // Collapsed frequency information, multi-phy, spec-an, etc
    std::shared_ptr<tracker_element_double_map> frequency_map;

    // Channel/freq content
    int channel_entry_id;

    int pack_comp_l1data, pack_comp_devinfo, pack_comp_common, pack_comp_device;

    int timer_id;
};

#endif

