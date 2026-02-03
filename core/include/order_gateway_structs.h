// mainly 4 structs are used in ordewr gateway 

// 1. The system_ack struct ===> this is ths structure in which matching engine sends acknowledgements
// 2. The user_ack struct   ===> this is the structure in which the usr expects acknowledgements
// 3. user_order_struct ===> structure in whihc we receive the order request from the usser, 
// 4. system_order_struct ===> this structure in which the order request expected by matching engine.
#pragma once

namespace internal_lib {

	/* We tryto make the structs memory friendly so they will be 16/32/24 byte so that integer number of these structs may fit into the cache line*/
	struct LOBOrder{
		// The structure of order expected by LOB. - 32 Byte.

		// 8 Byte
		long long arrived_at; // * byte

		// 8 byte
		int system_id; // unique id provided to this order by system   4 byte
		float price; // we expect price ~ 32k only (use case)    4 byte

		// 8 byte
		int quantity; // quantity of order 4 byte
		short int trader_id; // id of trader ~ 2 Byte ( 100 user total ==> 1 sniper and 99 will be market makers, ids will be 0 based indexed)
		char order_type; // 'b' or 's' 1 byte
		char req_type; // 'c'-create, 'u'-update, 'd'-delete // 1 byte

		// 8 byte padding to make it 32 byte 
		char pad[8];
	};

	struct UserOrder{
		// The structure of order sent by User. - 32 Byte.
		
		// 8 Byte
		long long arrived_at; // 8 Byte (null as of now) btu as sopon as it gets popped out at the gateway we will set it to be time.now()

		// 8 byte
		long long order_id; // unique order is = created_at*100 + userid(0 to 99). thia makes it unique.
		
		// 8 Byte
		float price; // we expect price ~ 32k only (use case)    4 byte
		int quantity; // quantity of order 4 byte

		// 8 byte
		short trader_id; // id of trader ~ 2 Byte ( 100 user total ==> 1 sniper and 99 will be market makers, ids will be 0 based indexed)
		char order_type; // 'b' or 's' 1 byte
		char req_type; // 'c'-create, 'u'-update, 'd'-delete // 1 byte
		char pad[4];	// 4 byte
		
	};

	// OrderStatus Class
	enum class OrderStatus : char {
        ACCEPTED = 'A', // order rested
        CANCELED = 'C', // order killed
        UPDATED  = 'U', // order modified
        FILLED   = 'F', // trade full
        PARTIAL  = 'P', // trade partial
        REJECTED = 'R'  // error
    };

	struct LOBAcknowledgement {
        //  8 Bytes
        long long timestamp;    // when did this happen inside LOB

        // 8 bytes
        int system_id;          // 4 byte internal id
        int filled_quantity;    // 4 byte  executed or remaining quantity

        // 8 bytes
        int price;              // 4 byte execution or limit price
        OrderStatus status;     // 1 byte 
        char side;              // 1 byte
        short traderId;   // 2 byte 

        // 8 bytes
        char pad_final[8];      //  padding to reach 32 bytes
    };

    // majorly there are 4 types of Logs 


    struct UserAcknowledgement {
        //   8 bytes
        long long order_id;     // client ID (translated from system_id) uing look up table (LUT)

        //  8 bytes
        long long timestamp;    // copied from LOBAcknowledgement

        // 8 bytes
        int filled_quantity;    // 4 byte
        int price;              // 4 byte

        //  8 bytes
        OrderStatus status;     // 1 byte
        char side;              // 1 byte
        char pad[6];            // 6 byte -- Fill to 32 byte
    };
}