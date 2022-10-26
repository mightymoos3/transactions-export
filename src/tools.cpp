//
// Created by marcin on 5/11/15.
//

#include "tools.h"



namespace xmreg
{

    /**
     * Parse key string, e.g., a viewkey in a string
     * into crypto::secret_key or crypto::public_key
     * depending on the template argument.
     */
    template <typename T>
    bool
    parse_str_secret_key(const string& key_str, T& secret_key)
    {

        // hash and keys have same structure, so to parse string of
        // a key, e.g., a view key, we can first parse it into the hash
        // object using parse_hash256 function, and then copy the reslting
        // hash data into secret key.
        crypto::hash hash_;

        if(!parse_hash256(key_str, hash_))
        {
            cerr << "Cant parse a key (e.g. viewkey): " << key_str << endl;
            return false;
        }

        // crypto::hash and crypto::secret_key have basicly same
        // structure. They both keep they key/hash as c-style char array
        // of fixed size. Thus we can just copy data from hash
        // to key
        copy(begin(hash_.data), end(hash_.data), secret_key.data);

        return true;
    }

    // explicit instantiations of get template function
    template bool parse_str_secret_key<crypto::secret_key>(const string& key_str, crypto::secret_key& secret_key);
    template bool parse_str_secret_key<crypto::public_key>(const string& key_str, crypto::public_key& secret_key);
    template bool parse_str_secret_key<crypto::hash>(const string& key_str, crypto::hash& secret_key);

    /**
     * Get transaction tx using given tx hash. Hash is represent as string here,
     * so before we can tap into the blockchain, we need to pare it into
     * crypto::hash object.
     */
    bool
    get_tx_pub_key_from_str_hash(Blockchain& core_storage, const string& hash_str, transaction& tx)
    {
        crypto::hash tx_hash;
        parse_hash256(hash_str, tx_hash);

        try
        {
            // get transaction with given hash
            tx = core_storage.get_db().get_tx(tx_hash);
        }
        catch (const TX_DNE& e)
        {
            cerr << e.what() << endl;
            return false;
        }

        return true;
    }



    string
    print_sig (const signature& sig)
    {
        stringstream ss;

        ss << "c: <" << epee::string_tools::pod_to_hex(sig.c) << "> "
           << "r: <" << epee::string_tools::pod_to_hex(sig.r) << ">";

        return ss.str();
    }

    /**
     * Check if a character is a path seprator
     */
    inline bool
    is_separator(char c)
    {
        // default linux path separator
        const char separator = PATH_SEPARARTOR;

        return c == separator;
    }



    /**
     * Remove trailinig path separator.
     */
    string
    remove_trailing_path_separator(const string& in_path)
    {
        string new_string = in_path;
        if (!new_string.empty() && is_separator(new_string[new_string.size() - 1]))
            new_string.erase(new_string.size() - 1);
        return new_string;
    }

    bf::path
    remove_trailing_path_separator(const bf::path& in_path)
    {
        string path_str = in_path.native();
        return bf::path(remove_trailing_path_separator(path_str));
    }


    string
    timestamp_to_str(time_t timestamp, const char* format)
    {

        const int TIME_LENGTH = 60;

        char str_buff[TIME_LENGTH];

        tm *tm_ptr;
        tm_ptr = localtime(&timestamp);

        size_t len;

        len = std::strftime(str_buff, TIME_LENGTH, format, tm_ptr);

        return string(str_buff, len);
    }

    std::string
    my_get_account_address_as_str(
            network_type nettype, account_public_address const & adr)
    {
        uint64_t address_prefix = nettype ?
                                  config::testnet::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX
                                  : config::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX;

        return tools::base58::encode_addr(address_prefix, t_serializable_object_to_blob(adr));
    }

//    ostream&
//    operator<< (ostream& os, const address_parse_info& addr_info)
//    {
//        os << get_account_address_as_str(false, addr_info.is_subaddress, addr_info.address);
//        return os;
//    }



    /*
     * Generate key_image of foran ith output
     */
    bool
    generate_key_image(const crypto::key_derivation& derivation,
                       const std::size_t i,
                       const crypto::secret_key& sec_key,
                       const crypto::public_key& pub_key,
                       crypto::key_image& key_img)
    {

        cryptonote::keypair in_ephemeral;

        if (!crypto::derive_public_key(derivation, i,
                                       pub_key,
                                       in_ephemeral.pub))
        {
            cerr << "Error generating publick key " << pub_key << endl;
            return false;
        }

        try
        {

            crypto::derive_secret_key(derivation, i,
                                      sec_key,
                                      in_ephemeral.sec);
        }
        catch(const std::exception& e)
        {
            cerr << "Error generate secret image: " << e.what() << endl;
            return false;
        }


        try
        {
            crypto::generate_key_image(in_ephemeral.pub,
                                       in_ephemeral.sec,
                                       key_img);
        }
        catch(const std::exception& e)
        {
            cerr << "Error generate key image: " << e.what() << endl;
            return false;
        }

        return true;
    }


    string
    get_default_lmdb_folder(cryptonote::network_type nettype)
    {
        // default path to monero folder
        // on linux this is /home/<username>/.bitmonero
        string default_monero_dir = tools::get_default_data_dir();

        if (nettype == cryptonote::network_type::TESTNET)
            default_monero_dir += "/testnet";
        if (nettype == cryptonote::network_type::STAGENET)
            default_monero_dir += "/stagenet";


        // the default folder of the lmdb blockchain database
        // is therefore as follows
        return default_monero_dir + string("/lmdb");
    }



    /*
    * Ge blockchain exception from command line option
    *
    * If not given, provide default path
    */
    bool
    get_blockchain_path(const boost::optional<string>& bc_path,
                        bf::path& blockchain_path,
                        cryptonote::network_type nettype)
    {
        // the default folder of the lmdb blockchain database
        string default_lmdb_dir   = xmreg::get_default_lmdb_folder(nettype);

        blockchain_path = bc_path
                          ? bf::path(*bc_path)
                          : bf::path(default_lmdb_dir);

        if (!bf::is_directory(blockchain_path))
        {
            cerr << "Given path \"" << blockchain_path   << "\" "
                 << "is not a folder or does not exist" << " "
                 << endl;

            return false;
        }

        blockchain_path = xmreg::remove_trailing_path_separator(blockchain_path);

        return true;
    }


    uint64_t
    sum_money_in_outputs(const transaction& tx)
    {
        uint64_t sum_xmr {0};

        for (const tx_out& txout: tx.vout)
        {
            sum_xmr += txout.amount;
        }

        return sum_xmr;
    }



    uint64_t
    sum_money_in_inputs(const transaction& tx)
    {
        uint64_t sum_xmr {0};

        size_t input_no = tx.vin.size();

        for (size_t i = 0; i < input_no; ++i)
        {

            if(tx.vin[i].type() != typeid(cryptonote::txin_to_key))
            {
                continue;
            }

            // get tx input key
            const cryptonote::txin_to_key& tx_in_to_key
                    = boost::get<cryptonote::txin_to_key>(tx.vin[i]);

            sum_xmr += tx_in_to_key.amount;
        }

        return sum_xmr;
    }



    array<uint64_t, 2>
    sum_money_in_tx(const transaction& tx)
    {
        array<uint64_t, 2> sum_xmr;

        sum_xmr[0] = sum_money_in_inputs(tx);
        sum_xmr[1] = sum_money_in_outputs(tx);

        return sum_xmr;
    };


    array<uint64_t, 2>
    sum_money_in_txs(const vector<transaction>& txs)
    {
        array<uint64_t, 2> sum_xmr {0,0};

        for (const transaction& tx: txs)
        {
            sum_xmr[0] += sum_money_in_inputs(tx);
            sum_xmr[1] += sum_money_in_outputs(tx);
        }

        return sum_xmr;
    };


    uint64_t
    sum_fees_in_txs(const vector<transaction>& txs)
    {
        uint64_t fees_sum {0};

        for (const transaction& tx: txs)
        {
            fees_sum += get_tx_fee(tx);
        }

        return fees_sum;
    }



    vector<pair<txout_to_key, uint64_t>>
    get_ouputs(const transaction& tx)
    {
        vector<pair<txout_to_key, uint64_t>> outputs;

        for (const tx_out& txout: tx.vout)
        {
            if (txout.target.type() != typeid(txout_to_key))
            {
                continue;
            }

            // get tx input key
            const txout_to_key& txout_key
                    = boost::get<cryptonote::txout_to_key>(txout.target);

            outputs.push_back(make_pair(txout_key, txout.amount));
        }

        return outputs;

    };

    vector<tuple<txout_to_key, uint64_t, uint64_t>>
    get_ouputs_tuple(const transaction& tx)
    {
        vector<tuple<txout_to_key, uint64_t, uint64_t>> outputs;

        for (uint64_t n = 0; n < tx.vout.size(); ++n)
        {

            if (tx.vout[n].target.type() != typeid(txout_to_key))
            {
                continue;
            }

            // get tx input key
            const txout_to_key& txout_key
                    = boost::get<cryptonote::txout_to_key>(tx.vout[n].target);

            outputs.push_back(make_tuple(txout_key, tx.vout[n].amount, n));
        }

        return outputs;
    };

    uint64_t
    get_mixin_no(const transaction& tx)
    {
        uint64_t mixin_no {0};

        size_t input_no = tx.vin.size();

        for (size_t i = 0; i < input_no; ++i)
        {

            if(tx.vin[i].type() != typeid(cryptonote::txin_to_key))
            {
                continue;
            }

            // get tx input key
            const txin_to_key& tx_in_to_key
                    = boost::get<cryptonote::txin_to_key>(tx.vin[i]);

            mixin_no = tx_in_to_key.key_offsets.size();

            // look for first mixin number.
            // all inputs in a single transaction have same number
            if (mixin_no > 0)
            {
                break;
            }
        }

        return mixin_no;
    }

    vector<uint64_t>
    get_mixin_no_in_txs(const vector<transaction>& txs)
    {
        vector<uint64_t> mixin_no;

        for (const transaction& tx: txs)
        {
            mixin_no.push_back(get_mixin_no(tx));
        }

        return mixin_no;
    }


    vector<txin_to_key>
    get_key_images(const transaction& tx)
    {
        vector<txin_to_key> key_images;

        size_t input_no = tx.vin.size();

        for (size_t i = 0; i < input_no; ++i)
        {

            if(tx.vin[i].type() != typeid(txin_to_key))
            {
                continue;
            }

            // get tx input key
            const txin_to_key& tx_in_to_key
                    = boost::get<cryptonote::txin_to_key>(tx.vin[i]);

            key_images.push_back(tx_in_to_key);
        }

        return key_images;
    }


    bool
    get_payment_id(const vector<uint8_t>& extra,
                   crypto::hash& payment_id,
                   crypto::hash8& payment_id8)
    {
        payment_id = null_hash;
        payment_id8 = null_hash8;

        std::vector<tx_extra_field> tx_extra_fields;

        if(!parse_tx_extra(extra, tx_extra_fields))
        {
            return false;
        }

        tx_extra_nonce extra_nonce;

        if (find_tx_extra_field_by_type(tx_extra_fields, extra_nonce))
        {
            // first check for encrypted id and then for normal one
            if(get_encrypted_payment_id_from_tx_extra_nonce(extra_nonce.nonce, payment_id8))
            {
                return true;
            }
            else if (get_payment_id_from_tx_extra_nonce(extra_nonce.nonce, payment_id))
            {
                return true;
            }
        }

        return false;
    }


    bool
    get_payment_id(const transaction& tx,
                   crypto::hash& payment_id,
                   crypto::hash8& payment_id8)
    {
        return get_payment_id(tx.extra, payment_id, payment_id8);
    }


    /**
     * Rough estimate of block height from the time provided
     *
     */
    uint64_t
    estimate_bc_height(const string& date, const char* format)
    {
        const pt::ptime MONERO_START {gt::date(2014,04,18)};
        const uint64_t MONERO_BLOCK_TIME {60}; // seconds

        dateparser parser {format};

        if (!parser(date))
        {
           throw runtime_error(string("Date format is incorrect: ") + date);
        }

        pt::ptime requested_date = parser.pt;

        if (requested_date < MONERO_START)
        {
            return 0;
        }

        pt::time_duration td = requested_date - MONERO_START;

        return static_cast<uint64_t>(td.total_seconds()) / MONERO_BLOCK_TIME;
    }


    array<size_t, 5>
    timestamp_difference(uint64_t t1, uint64_t t2)
    {

        uint64_t timestamp_diff = t1 - t2;

        // calculate difference of timestamps from current block to the mixin one
        if (t2 > t1)
        {
            timestamp_diff = t2 - t1;
        }

        uint64_t time_diff_years = timestamp_diff / 31536000;

        timestamp_diff -=  time_diff_years * 31536000;

        uint64_t time_diff_days = timestamp_diff / 86400;

        timestamp_diff -=  time_diff_days * 86400;

        uint64_t time_diff_hours = timestamp_diff / 3600;

        timestamp_diff -=  time_diff_hours * 3600;

        uint64_t time_diff_minutes = timestamp_diff / 60;

        timestamp_diff -=  time_diff_minutes * 60;

        uint64_t time_diff_seconds = timestamp_diff ;

        return array<size_t, 5> {time_diff_years, time_diff_days,
                                 time_diff_hours, time_diff_minutes,
                                 time_diff_seconds};

    };


    string
    read(string filename)
    {
       if (!bf::exists(bf::path(filename)))
       {
           cerr << "File does not exist: " << filename << endl;
           return string();
       }

        std::ifstream t(filename);
        return string(std::istreambuf_iterator<char>(t),
                      std::istreambuf_iterator<char>());
    }

    pair<string, double>
    timestamps_time_scale(const vector<uint64_t>& timestamps,
                          uint64_t timeN,
                          uint64_t resolution,
                          uint64_t time0)
    {
        string empty_time =  string(resolution, '_');

        size_t time_axis_length = empty_time.size();

        uint64_t interval_length = timeN-time0;

        double scale = double(interval_length) / double(time_axis_length);

        for (const auto& timestamp: timestamps)
        {

            if (timestamp < time0 || timestamp > timeN)
            {
                cout << "Out of range" << endl;
                continue;
            }

            uint64_t timestamp_place = double(timestamp-time0)
                             / double(interval_length)*(time_axis_length - 1);

            empty_time[timestamp_place + 1] = '*';
        }

        return make_pair(empty_time, scale);
    }


    bool
    decode_ringct(const rct::rctSig& rv,
                  const crypto::public_key& pub,
                  const crypto::secret_key& sec,
                  unsigned int i,
                  rct::key& mask,
                  uint64_t& amount)
    {
        crypto::key_derivation derivation;

        bool r = crypto::generate_key_derivation(pub, sec, derivation);

        if (!r)
        {
            cerr <<"Failed to generate key derivation to decode rct output " << i << endl;
            return false;
        }

        crypto::secret_key scalar1;

        crypto::derivation_to_scalar(derivation, i, scalar1);

        try
        {
            switch (rv.type)
            {
                case rct::RCTTypeSimple:
                case rct::RCTTypeBulletproof:
                case rct::RCTTypeBulletproof2:
                case rct::RCTTypeCLSAG:
                case rct::RCTTypeBulletproofPlus:
                    amount = rct::decodeRctSimple(rv,
                                                  rct::sk2rct(scalar1),
                                                  i,
                                                  mask,
                                                  hw::get_device("default"));
                    break;
                case rct::RCTTypeFull:
                    amount = rct::decodeRct(rv,
                                            rct::sk2rct(scalar1),
                                            i,
                                            mask,
                                            hw::get_device("default"));
                    break;
                default:
                    cerr << "Unsupported rct type: " << rv.type << endl;
                    return false;
            }
        }
        catch (const std::exception &e)
        {
            cerr << "Failed to decode input " << i << endl;
            return false;
        }

        return true;
    }

    bool
    url_decode(const std::string& in, std::string& out)
    {
        out.clear();
        out.reserve(in.size());
        for (std::size_t i = 0; i < in.size(); ++i)
        {
            if (in[i] == '%')
            {
                if (i + 3 <= in.size())
                {
                    int value = 0;
                    std::istringstream is(in.substr(i + 1, 2));
                    if (is >> std::hex >> value)
                    {
                        out += static_cast<char>(value);
                        i += 2;
                    }
                    else
                    {
                        return false;
                    }
                }
                else
                {
                    return false;
                }
            }
            else if (in[i] == '+')
            {
                out += ' ';
            }
            else
            {
                out += in[i];
            }
        }
        return true;
    }

    map<std::string, std::string>
    parse_crow_post_data(const string& req_body)
    {
        map<std::string, std::string> body;

        vector<string> vec;
        string tmp;
        bool result = url_decode(req_body, tmp);
        if (result)
        {
            boost::algorithm::split(vec, tmp, [](char x) {return x == '&'; });
            for(auto &it : vec)
            {
                auto pos = it.find("=");
                if (pos != string::npos)
                {
                    body[it.substr(0, pos)] = it.substr(pos + 1);
                }
                else
                {
                    break;
                }
            }
        }
        return body;
    }


    time_t
    to_time_t(pt::ptime t)
    {
        using namespace boost::posix_time;
        ptime epoch(boost::gregorian::date(1970,1,1));
        time_duration::sec_type x = (t - epoch).total_seconds();
        return time_t(x);
    }


    // based on
    // crypto::public_key wallet2::get_tx_pub_key_from_received_outs(const tools::wallet2::transfer_details &td) const
    public_key
    get_tx_pub_key_from_received_outs(const transaction &tx)
    {
        std::vector<tx_extra_field> tx_extra_fields;

        if(!parse_tx_extra(tx.extra, tx_extra_fields))
        {
            // Extra may only be partially parsed, it's OK if tx_extra_fields contains public key
        }

        // Due to a previous bug, there might be more than one tx pubkey in extra, one being
        // the result of a previously discarded signature.
        // For speed, since scanning for outputs is a slow process, we check whether extra
        // contains more than one pubkey. If not, the first one is returned. If yes, they're
        // checked for whether they yield at least one output
        tx_extra_pub_key pub_key_field;

        if (!find_tx_extra_field_by_type(tx_extra_fields, pub_key_field, 0))
        {
            return null_pkey;
        }

        public_key tx_pub_key = pub_key_field.pub_key;

        bool two_found = find_tx_extra_field_by_type(tx_extra_fields, pub_key_field, 1);

        if (!two_found)
        {
            // easy case, just one found
            return tx_pub_key;
        }
        else
        {
            // just return second one if there are two.
            // this does not require private view key, as
            // its not needed for my use case.
            return pub_key_field.pub_key;
        }

        return null_pkey;
    }


    /**
    * Return string representation of monero address
    */
    string
    print_address(const address_parse_info& address_info, cryptonote::network_type nettype)
    {
        return "<" + get_account_address_as_str(
                nettype, address_info.is_subaddress, address_info.address)
               + ">";
    }


}

