#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <endian.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "hash.h"
#include "client.h"

/* third party libraries */
#include <openssl/evp.h>

/* You shouldn't have to be looking at this file, but have fun! */


struct sha1sum_ctx {
	EVP_MD_CTX *ctx;
	const EVP_MD *md;
	uint8_t *salt;
	size_t len;
};


struct sha1sum_ctx * sha1sum_create(const uint8_t *salt, size_t len) {
	struct sha1sum_ctx *csm = (struct sha1sum_ctx *)malloc(sizeof(*csm));
	if (!csm) {
		goto err;
	}
	bzero(csm, sizeof(*csm));
	csm->ctx = EVP_MD_CTX_new();
	csm->md = EVP_sha1();
	csm->len = len;
	if (len > 0) {
		csm->salt = (uint8_t *)malloc(len);
		if (!csm->salt) {
			goto err;
		}
		memcpy(csm->salt, salt, len);
	}
	if (sha1sum_reset(csm)) {
		goto err;
	}


	return csm;

  err:
	if (csm) {
		if (csm->salt) {
			free(csm->salt);
		}
		bzero(csm, sizeof(*csm));
		free(csm);
	}
	csm = NULL;

	return csm;
}

int sha1sum_update(struct sha1sum_ctx *csm, const uint8_t *payload, size_t len) {
	return EVP_DigestUpdate(csm->ctx, payload, len) != 1;
}

int sha1sum_finish(struct sha1sum_ctx *csm, const uint8_t *payload, size_t len, uint8_t *out) {
	int ret = 1;
	if (len) {
		ret = EVP_DigestUpdate(csm->ctx, payload, len);
	}
	if (ret == 1) {
		return EVP_DigestFinal_ex(csm->ctx, out, NULL) != 1;
	} else {
		return 1;
	}
}

int sha1sum_reset(struct sha1sum_ctx *csm) {
	EVP_DigestInit_ex(csm->ctx, csm->md, NULL);
	if (csm->len) {
		return EVP_DigestUpdate(csm->ctx, csm->salt, csm->len) != 1;
	} else {
		return 0;
	}
	
}

int sha1sum_destroy(struct sha1sum_ctx *csm) {
	EVP_MD_CTX_free(csm->ctx);
	free(csm->salt);
	bzero(csm, sizeof(*csm));
	free(csm);
	return 0;
}

int sha1_hash(uint8_t *out, uint8_t *payload, size_t payload_length) {

	/* creates sha1sum */
	struct sha1sum_ctx *ctx = sha1sum_create(NULL, 0);
	if (ctx == NULL) {
		fprintf(stderr, "Error creating checksum\n");
		return 0;
	}

	/* sha1sum_finish */
	int error = sha1sum_finish(ctx, payload, payload_length, out);
	if (error != 0) {
		fprintf(stderr, "Error sha1sum_finish\n");
		return 0;
	}

	return 1;
}


// returns -1 if the hashes are not the same; 1 if they are
int compare_piece_hash(Piece *piece, int piece_index) {

	ssize_t piece_size = metafile_message.piece_length;
	if (piece_index == num_pieces-1) {
		piece_size = metafile_message.total_length-metafile_message.piece_length*18;
	}
	if (piece->isComplete != 1) {
		puts("Called this function when piece is not complete");
		return -1;
	}

	// reads in the entire file for the piece
    int c;
    int char_index = 0;
    uint8_t file_buf[piece_size];
    if(file_buf == NULL) {
        puts ("failed file_Buf malloc");
        exit(1);
    }

	fseek(piece->fp, 0, SEEK_SET);

	while ((c = fgetc(piece->fp)) != EOF) {
        file_buf[char_index++] = c;
    }

	// printf("CHAR_INDEX IS: %d\n", char_index);
	//fwrite(file_buf, 1, char_index, stdout);
	// printf("\n");

	//gets the hashed value of the piece
    uint8_t hashed_piece[20];
    sha1_hash(hashed_piece, file_buf, char_index);

	//compares the hash from the metafile with hashed_piece and returns -1 if the hashes are not the same
	int index = 20 * piece_index;
	int count = 0;

	// puts("THIS IS THE REAL HASH OF THE PIECE WE WANT:");
	// fwrite(&metafile_message.hash_list[index], 1, 20, stdout);
    // printf("\n");
	// puts("THIS IS THE HASH PIECE FROM FILE:");
	// fwrite(hashed_piece, 1, 20, stdout);
    // printf("\n");

	while (count < 20 && index < metafile_message.hash_list_length) {
		if (metafile_message.hash_list[index++] != hashed_piece[count++]) {

			//sets the file pointer to the beginning again to be rewritten
			fseek(piece->fp, 0, SEEK_SET);
			return -1;
		}
	}

	return 1;
}
