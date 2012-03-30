#include "clar_libgit2.h"

#include "tree.h"

static const char *blob_oid = "fa49b077972391ad58037050f2a75f74e3671e92";
static const char *first_tree  = "181037049a54a1eb5fab404658a3a250b44335d7";
static const char *second_tree = "f60079018b664e4e79329a7ef9559c8d9e0378d1";
static const char *third_tree = "eb86d8b81d6adbd5290a935d6c9976882de98488";

static git_repository *g_repo;


// Helpers
static int print_tree(git_repository *repo, const git_oid *tree_oid, int depth)
{
	static const char *indent = "                              ";
	git_tree *tree;
	unsigned int i;

	if (git_tree_lookup(&tree, repo, tree_oid) < GIT_SUCCESS)
		return GIT_ERROR;

	for (i = 0; i < git_tree_entrycount(tree); ++i) {
		const git_tree_entry *entry = git_tree_entry_byindex(tree, i);
		char entry_oid[40];

		git_oid_fmt(entry_oid, &entry->oid);
		printf("%.*s%o [%.*s] %s\n", depth*2, indent, entry->attr, 40, entry_oid, entry->filename);

		if (entry->attr == S_IFDIR) {
			if (print_tree(repo, &entry->oid, depth + 1) < GIT_SUCCESS) {
				git_tree_free(tree);
				return GIT_ERROR;
			}
		}
	}

	git_tree_free(tree);
	return GIT_SUCCESS;
}

static void locate_loose_object(const char *repository_folder, git_object *object, char **out, char **out_folder)
{
	static const char *objects_folder = "objects/";

	char *ptr, *full_path, *top_folder;
	int path_length, objects_length;

	assert(repository_folder && object);

	objects_length = strlen(objects_folder);
	path_length = strlen(repository_folder);
	ptr = full_path = git__malloc(path_length + objects_length + GIT_OID_HEXSZ + 3);

	strcpy(ptr, repository_folder);
	strcpy(ptr + path_length, objects_folder);

	ptr = top_folder = ptr + path_length + objects_length;
	*ptr++ = '/';
	git_oid_pathfmt(ptr, git_object_id(object));
	ptr += GIT_OID_HEXSZ + 1;
	*ptr = 0;

	*out = full_path;

	if (out_folder)
		*out_folder = top_folder;
}

static int loose_object_mode(const char *repository_folder, git_object *object)
{
	char *object_path;
	struct stat st;

	locate_loose_object(repository_folder, object, &object_path, NULL);
	if (p_stat(object_path, &st) < 0)
		return 0;
	free(object_path);

	return st.st_mode;
}

static int loose_object_dir_mode(const char *repository_folder, git_object *object)
{
	char *object_path;
	size_t pos;
	struct stat st;

	locate_loose_object(repository_folder, object, &object_path, NULL);

	pos = strlen(object_path);
	while (pos--) {
		if (object_path[pos] == '/') {
			object_path[pos] = 0;
			break;
		}
	}

	if (p_stat(object_path, &st) < 0)
		return 0;
	free(object_path);

	return st.st_mode;
}


// Fixture setup and teardown
void test_object_tree_write__initialize(void)
{
   g_repo = cl_git_sandbox_init("testrepo");
}

void test_object_tree_write__cleanup(void)
{
   cl_git_sandbox_cleanup();
}


#if 0
void xtest_object_tree_write__print(void)
{
   // write a tree from an index
	git_index *index;
	git_oid tree_oid;

	cl_git_pass(git_repository_index(&index, g_repo));

	cl_git_pass(git_tree_create_fromindex(&tree_oid, index));
	cl_git_pass(print_tree(g_repo, &tree_oid, 0));
}
#endif

void test_object_tree_write__from_memory(void)
{
   // write a tree from a memory
	git_treebuilder *builder;
	git_tree *tree;
	git_oid id, bid, rid, id2;

	git_oid_fromstr(&id, first_tree);
	git_oid_fromstr(&id2, second_tree);
	git_oid_fromstr(&bid, blob_oid);

	//create a second tree from first tree using `git_treebuilder_insert` on REPOSITORY_FOLDER.
	cl_git_pass(git_tree_lookup(&tree, g_repo, &id));
	cl_git_pass(git_treebuilder_create(&builder, tree));

	cl_git_fail(git_treebuilder_insert(NULL, builder, "", &bid, 0100644));
	cl_git_fail(git_treebuilder_insert(NULL, builder, "/", &bid, 0100644));
	cl_git_fail(git_treebuilder_insert(NULL, builder, "folder/new.txt", &bid, 0100644));

	cl_git_pass(git_treebuilder_insert(NULL,builder,"new.txt",&bid,0100644));
	cl_git_pass(git_treebuilder_write(&rid, g_repo, builder));

	cl_assert(git_oid_cmp(&rid, &id2) == 0);

	git_treebuilder_free(builder);
	git_tree_free(tree);
}

void test_object_tree_write__subtree(void)
{
   // write a hierarchical tree from a memory
	git_treebuilder *builder;
	git_tree *tree;
	git_oid id, bid, subtree_id, id2, id3;
	git_oid id_hiearar;

	git_oid_fromstr(&id, first_tree);
	git_oid_fromstr(&id2, second_tree);
	git_oid_fromstr(&id3, third_tree);
	git_oid_fromstr(&bid, blob_oid);

	//create subtree
	cl_git_pass(git_treebuilder_create(&builder, NULL));
	cl_git_pass(git_treebuilder_insert(NULL,builder,"new.txt",&bid,0100644));
	cl_git_pass(git_treebuilder_write(&subtree_id, g_repo, builder));
	git_treebuilder_free(builder);

	// create parent tree
	cl_git_pass(git_tree_lookup(&tree, g_repo, &id));
	cl_git_pass(git_treebuilder_create(&builder, tree));
	cl_git_pass(git_treebuilder_insert(NULL,builder,"new",&subtree_id,040000));
	cl_git_pass(git_treebuilder_write(&id_hiearar, g_repo, builder));
	git_treebuilder_free(builder);
	git_tree_free(tree);

	cl_assert(git_oid_cmp(&id_hiearar, &id3) == 0);

	// check data is correct
	cl_git_pass(git_tree_lookup(&tree, g_repo, &id_hiearar));
	cl_assert(2 == git_tree_entrycount(tree));
#ifndef GIT_WIN32
   // TODO: fix these
	//cl_assert((loose_object_dir_mode("testrepo", (git_object *)tree) & 0777) == GIT_OBJECT_DIR_MODE);
	//cl_assert((loose_object_mode("testrespo", (git_object *)tree) & 0777) == GIT_OBJECT_FILE_MODE);
#endif
	git_tree_free(tree);
}
