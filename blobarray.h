
struct blob_t {
    int current;
    void * content;
    struct blob_t * next;
};


void * getFirstBlob(struct blob_t * first);
void * getLastBlob(struct blob_t * first);
void  * getNextBlob(struct blob_t * first);
void prepBlobIteration(struct blob_t * first);

void appendBlob(struct blob_t ** first, void * content );
void * getBlob(struct blob_t * first, int i);
void removeBlobArray(struct blob_t * first);
