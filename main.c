#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <rdma/fabric.h>
#include <rdma/fi_rma.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_cm.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define ERR(call, retv) \
	do { fprintf(stderr, call "(): %s:%d, ret=%d (%s)\n", __FILE__, __LINE__, \
			(int) (retv), fi_strerror((int) -(retv))); } while (0)



static void print_long_info(struct fi_info *info)
{
	struct fi_info *cur;
	for (cur = info; cur; cur = cur->next) {
        printf(fi_tostr(cur, FI_TYPE_INFO));
    }
}

static void print_short_info(struct fi_info *info) {
	struct fi_info *cur;
	for (cur = info; cur; cur = cur->next) {
		printf("provider: %s\n", cur->fabric_attr->prov_name);
		printf("    fabric: %s\n", cur->fabric_attr->name),
		printf("    domain: %s\n", cur->domain_attr->name),
		printf("    version: %d.%d\n", FI_MAJOR(cur->fabric_attr->prov_version),
        FI_MINOR(cur->fabric_attr->prov_version));
        printf("    type: %s\n", fi_tostr(&cur->ep_attr->type, FI_TYPE_EP_TYPE));
        printf("    protocol: %s\n", fi_tostr(&cur->ep_attr->protocol, FI_TYPE_PROTOCOL));
        printf("    addr format: %s\n", fi_tostr(&cur->addr_format, FI_TYPE_ADDR_FORMAT));
	}
}

struct fi_info* get_info() {
    struct fi_info *fi, *hints;
    int rc;

    hints = fi_allocinfo();
    hints->mode = FI_LOCAL_MR;
    hints->caps = FI_RMA;
    //hints->ep_attr->type = FI_EP_RDM;
    hints->ep_attr->type = FI_EP_MSG;
    hints->fabric_attr->prov_name = strdup("tcp");

    rc = fi_getinfo(FI_VERSION(1, 4), "0.0.0.0", NULL, 0, hints, &fi);

    fi_freeinfo(hints);

    if (rc) {
        fprintf(stderr, "cannot get fabric info, rc: %d\n", rc);
        return NULL;
    }

    return fi;
}

void free_info(struct fi_info *info) {
    fi_freeinfo(info);
}

void print_ep_name(struct fid_ep *ep) {
    void *addr;
    struct sockaddr_in *sin;
    size_t addrlen = 0;

    printf("print_ep_name");

    fi_getname((fid_t)ep, NULL, &addrlen);

    addr = malloc(addrlen);
    fi_getname((fid_t)ep, addr, &addrlen);

    sin = (struct sockaddr_in *)addr;

    printf("ep addr: %s:%d\n", inet_ntoa(sin->sin_addr), sin->sin_port);

    free(addr);
    printf("freed\n");
}

int main() {
    struct fid_fabric *fabric;
    struct fid_domain *domain;
    struct fid_ep *ep;
    struct fid_mr *mr;
    struct fi_eq_attr eq_attr = {
        .wait_obj = FI_WAIT_UNSPEC
    };
    struct fid_eq *eq;

    struct fi_info *fi = get_info();
    int rc = 0;

    if (!fi) {
        return -1;
    }

    print_short_info(fi);

    rc = fi_fabric(fi->fabric_attr, &fabric, NULL);
    if (rc < 0) {
        ERR("fi_fabric", rc);
        goto done;
    }

    rc = fi_domain(fabric, fi, &domain, NULL);
    if (rc < 0) {
        ERR("fi_fabric", rc);
        goto done1;
    }


    rc = fi_eq_open(fabric, &eq_attr, &eq, NULL);
	if (rc < 0) {
        ERR("fi_eq_open", rc);
		goto done2;
	}

    rc = fi_endpoint(domain, fi, &ep, NULL);
    if (rc < 0) {
        ERR("fi_endpoint", rc);
        goto done3;
    }

    rc = fi_ep_bind(ep, (fid_t)eq, FI_RECV);

    print_ep_name(ep);

done4:
    printf("ep %p\n", ep);
    fi_close((struct fid *)ep);
done3:
    printf("eq\n");
    fi_close((struct fid *)eq);
done2:
    printf("domain\n");
    fi_close((struct fid *)domain);
done1:
    printf("fabric\n");
    fi_close((struct fid *)fabric);
done:
    free_info(fi);

    return rc;
}
